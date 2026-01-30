#include "frameHandler.h"
#include "allianceHandler.h"


void processFrame(Maester *maester, Frame *frame, int fromSocket) {
    if (!maester || !frame) {
        return;
    }
    
    if (isDestination(maester, frame->ip_destination)) {
        // Soy el destinatario, procesar según tipo
        char *msg;
        asprintf(&msg, GREEN "Frame is for me, processing TYPE=0x%02X\n" RESET, frame->type);
        customWrite(1, msg);
        free(msg);
        
        switch (frame->type) {
            case PING_PONG:
                //handlePingPong(maester, frame, fromSocket);
                break;
                
            case MAESTER_DISCONNECT:
                handleDisconnect(maester, frame);
                break;
                
            case NACK_ERROR:
                handleNack(maester, frame, fromSocket);
                break;
                
            case ALLIANCE_REQUEST:
                handleAllianceRequest(maester, frame, fromSocket);
                break;
                
            case SIGIL_SEND:
                handleSigilSend(maester, frame, fromSocket);
                break;
                
            case ALLIANCE_RESPONSE:
                handleAllianceResponse(maester, frame, fromSocket);
                break;
                
            case PRODUCT_LIST_REQUEST:
                handleProductListRequest(maester, frame, fromSocket);
                break;
                
            case ACK_FILE:
                handleAckFile(maester, frame, fromSocket);
                break;
                
            case ACK_MD5SUM:
                handleAckMD5(maester, frame, fromSocket);
                break;
                
            default:
                sendNack(fromSocket, maester->name, "UNKNOWN_TYPE");
                break;
        }
    } else {
        // No soy destinatario, reenviar
        char *msg;
        asprintf(&msg, YELLOW "Frame not for me, forwarding to [%s]\n" RESET, frame->ip_destination);
        customWrite(1, msg);
        free(msg);
        
        forwardFrame(maester, frame, fromSocket);
    }
}

// ═══════════════════════════════════════════════════════════
// HANDLERS BÁSICOS
// ═══════════════════════════════════════════════════════════

void handleDisconnect(Maester *maester, Frame *frame) {
    
    char *msg;
    asprintf(&msg, YELLOW "[%s] has disconnected gracefully.\n" RESET, frame->ip_origin);
    customWrite(1, msg);
    free(msg);
    
    // Actualizar estado de alianzas si existe
    pthread_mutex_lock(&maester->alliances_mutex);
    for (int i = 0; i < maester->numAlliances; i++) {
        if (strcmp(maester->alliances[i].name, frame->ip_origin) == 0) {
            maester->alliances[i].status = ALLIANCE_NONE;
            asprintf(&msg, YELLOW "Alliance with [%s] marked as inactive\n" RESET, frame->ip_origin);
            customWrite(1, msg);
            free(msg);
            break;
        }
    }
    pthread_mutex_unlock(&maester->alliances_mutex);
}

void handleNack(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)fromSocket;
    
    char *msg;
    asprintf(&msg, RED "Els corbs s'han perdut - NACK from realm [%s]\n" RESET, frame->data);
    customWrite(1, msg);
    free(msg);
}

// ═══════════════════════════════════════════════════════════
// HANDLERS DE ALIANZA (Stubs por ahora)
// ═══════════════════════════════════════════════════════════

void handleAllianceRequest(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)frame;
    (void)fromSocket;
    
    customWrite(1, YELLOW "TODO: handleAllianceRequest not yet implemented\n" RESET);
}

void handleSigilSend(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)frame;
    (void)fromSocket;
    
    customWrite(1, YELLOW "TODO: handleSigilSend not yet implemented\n" RESET);
}

void handleAllianceResponse(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)frame;
    (void)fromSocket;
    
    customWrite(1, YELLOW "TODO: handleAllianceResponse not yet implemented\n" RESET);
}

// ═══════════════════════════════════════════════════════════
// HANDLERS DE PRODUCTOS
// ═══════════════════════════════════════════════════════════
void handleProductListRequest(Maester *maester, Frame *frame, int fromSocket) {
    char *msg;
    
    const char *originRealm = frame->ip_origin;
    
    asprintf(&msg, CYAN "Product list request from [%s]\n" RESET, originRealm);
    customWrite(1, msg);
    free(msg);
    
    // Verificar si tenim aliança amb aquest regne
    if (!hasAlliance(maester, originRealm)) {
        // No tenim aliança - enviar ERR_UNAUTHORIZED (0x25)
        asprintf(&msg, RED "No alliance with [%s] - sending UNAUTHORIZED\n" RESET, originRealm);
        customWrite(1, msg);
        free(msg);
        
        char myIpPort[IP_SIZE];
        snprintf(myIpPort, IP_SIZE, "%s:%d", maester->ip, maester->port);
        
        char errorData[DATA_MAX_SIZE];
        snprintf(errorData, DATA_MAX_SIZE, "AUTH&%s", originRealm);
        
        Frame errorFrame;
        createFrame(&errorFrame, ERR_UNAUTHORIZED, myIpPort, originRealm, errorData);
        
        sendFrame(fromSocket, &errorFrame);
        return;
    }
    
    // Tenim aliança - per F2 simplement enviem ACK (sense processar realment)
    asprintf(&msg, GREEN "Alliance verified with [%s] - sending ACK (F2 stub)\n" RESET, originRealm);
    customWrite(1, msg);
    free(msg);
    
    // Enviar ACK simple (per F2)
    Frame ackFrame;
    char ackData[DATA_MAX_SIZE];
    snprintf(ackData, DATA_MAX_SIZE, "OK&%s", maester->name);
    createFrame(&ackFrame, ACK_FILE, "", "", ackData);
    sendFrame(fromSocket, &ackFrame);
}

// ═══════════════════════════════════════════════════════════
// HANDLERS DE ACK (Stubs por ahora)
// ═══════════════════════════════════════════════════════════

void handleAckFile(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)frame;
    (void)fromSocket;
    
    customWrite(1, YELLOW "TODO: handleAckFile not yet implemented\n" RESET);
}

void handleAckMD5(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)frame;
    (void)fromSocket;
    
    customWrite(1, YELLOW "TODO: handleAckMD5 not yet implemented\n" RESET);
}
