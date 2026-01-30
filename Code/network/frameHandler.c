#include "frameHandler.h"


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

void handlePingPong(Maester *maester, Frame *frame, int fromSocket) {
    (void)fromSocket; // No usado - respuesta directa al origen
    
    char *msg;
    asprintf(&msg, MAGENTA "PING received from [%s], sending PONG...\n" RESET, frame->ip_origin);
    customWrite(1, msg);
    free(msg);
    
    // Buscar la ruta al origen (puede ser directa o DEFAULT)
    Route *originRoute = findRoute(maester, frame->ip_origin);
    if (!originRoute) {
        // No hay ruta directa, intentar DEFAULT
        originRoute = getDefaultRoute(maester);
    }
    
    if (!originRoute) {
        asprintf(&msg, RED "ERROR | No route to origin [%s] for PONG\n" RESET, frame->ip_origin);
        customWrite(1, msg);
        free(msg);
        return;
    }
    
    // Conectar DIRECTAMENTE al origen (sin pasar por DEFAULT/hops)
    int pong_fd;
    if (connectToRealm(originRoute, &pong_fd) < 0) {
        asprintf(&msg, RED "Els corbs s'han perdut - Error [%s]\n" RESET, frame->ip_origin);
        customWrite(1, msg);
        free(msg);
        return;
    }
    
    // Crear PONG: invertir ORIGIN ↔ DESTINATION
    Frame pongFrame;
    memcpy(&pongFrame, frame, sizeof(Frame));
    strncpy(pongFrame.ip_origin, maester->name, IP_SIZE - 1);
    pongFrame.ip_origin[IP_SIZE - 1] = '\0';
    strncpy(pongFrame.ip_destination, frame->ip_origin, IP_SIZE - 1);
    pongFrame.ip_destination[IP_SIZE - 1] = '\0';
    
    // Recalcular checksum con los campos invertidos
    pongFrame.checksum = calcChecksum(&pongFrame);
    
    // Enviar PONG directamente
    if (sendFrame(pong_fd, &pongFrame) < 0) {
        customWrite(1, RED "ERROR | Failed to send PONG\n" RESET);
    } else {
        asprintf(&msg, GREEN "PONG sent directly to [%s]\n" RESET, frame->ip_origin);
        customWrite(1, msg);
        free(msg);
    }
    
    close(pong_fd);
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
