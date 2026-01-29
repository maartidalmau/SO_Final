#include "frameHandler.h"
#include <unistd.h>

void processFrame(Maester *maester, Frame *frame, int fromSocket) {
    if (!maester || !frame) {
        return;
    }
    
    // ¿Soy el destinatario?
    if (isDestination(maester, frame->ip_destination)) {
        // Soy el destinatario, procesar según tipo
        char *msg;
        asprintf(&msg, GREEN "Frame is for me, processing TYPE=0x%02X\n" RESET, frame->type);
        customWrite(1, msg);
        free(msg);
        
        switch (frame->type) {
            case PING_PONG:
                handlePingPong(maester, frame, fromSocket);
                break;
                
            case MAESTER_DISCONNECT:
                handleDisconnect(maester, frame, fromSocket);
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
                customWrite(1, RED "ERROR | Unknown frame type\n" RESET);
                
                // Enviar NACK por tipo desconocido
                Frame nackFrame;
                createFrame(&nackFrame, NACK_ERROR, maester->name, frame->ip_origin, "Unknown frame type");
                sendFrame(fromSocket, &nackFrame);
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

void handlePingPong(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester; // No usado en esta implementación
    
    char *msg;
    asprintf(&msg, MAGENTA "PING received from [%s], sending PONG...\n" RESET, frame->ip_origin);
    customWrite(1, msg);
    free(msg);
    
    // Responder con el mismo frame (echo)
    if (sendFrame(fromSocket, frame) < 0) {
        customWrite(1, RED "ERROR | Failed to send PONG\n" RESET);
    } else {
        customWrite(1, GREEN "PONG sent successfully\n" RESET);
    }
}

void handleDisconnect(Maester *maester, Frame *frame, int fromSocket) {
    (void)fromSocket; // No usado
    
    char *msg;
    asprintf(&msg, YELLOW "[%s] has disconnected gracefully.\n" RESET, frame->ip_origin);
    customWrite(1, msg);
    free(msg);
    
    // Actualizar estado de alianzas si existe
    pthread_mutex_lock(&maester->alliances_mutex);
    for (int i = 0; i < maester->numAlliances; i++) {
        if (strcmp(maester->alliances[i].name, frame->ip_origin) == 0) {
            maester->alliances[i].status = 0; // Inactive
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
    asprintf(&msg, RED "NACK received from [%s]: %s\n" RESET, 
             frame->ip_origin, frame->data);
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
// HANDLERS DE PRODUCTOS (Stubs por ahora)
// ═══════════════════════════════════════════════════════════

void handleProductListRequest(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)frame;
    (void)fromSocket;
    
    customWrite(1, YELLOW "TODO: handleProductListRequest not yet implemented\n" RESET);
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
