#include "client.h"


int sendAllianceRequest(Maester *maester, const char *realmName, const char *sigilPath) {
    if (!maester || !realmName || !sigilPath) {
        return -1;
    }
    char *msg;
    
    // 1. Verificar que no tenim ja aliança activa amb aquest regne (thread-safe)
    int existingStatus = ALLIANCE_NONE;
    if (getAllianceInfo(maester, realmName, NULL, NULL, &existingStatus, NULL)) {
        if (existingStatus == ALLIANCE_ACTIVE) {
            asprintf(&msg, YELLOW "Already allied with [%s]\n" RESET, realmName);
            customWrite(1, msg);
            free(msg);
            return 0;
        }
        if (existingStatus == ALLIANCE_PENDING) {
            asprintf(&msg, YELLOW "Alliance request to [%s] already pending\n" RESET, realmName);
            customWrite(1, msg);
            free(msg);
            return 0;
        }
    }
    
    // 2. Buscar ruta al regne (directa o via DEFAULT) - thread-safe
    char *routeIp = NULL;
    int routePort = 0;
    
    if (!getRouteInfo(maester, realmName, &routeIp, &routePort) || 
        (routeIp && strcasecmp(routeIp, "*.*.*.*") == 0)) {
        // No direct route or unknown IP, try DEFAULT
        if (routeIp) free(routeIp);
        routeIp = NULL;
        if (!getRouteInfo(maester, NULL, &routeIp, &routePort)) {
            asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
            customWrite(1, msg);
            free(msg);
            return -1;
        }
    }
    
    if (!routeIp) {
        asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
    // 3. Connectar al següent hop
    int fd_nextHop = -1;
    if (connectToRealmByRoute(routeIp, routePort, &fd_nextHop) < 0) {
        asprintf(&msg, RED "ERROR | Cannot connect to route for [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        free(routeIp);
        return -1;
    }
    
    free(routeIp);  // Ya no necesitamos la IP
    
    // 4. Crear la trama ALLIANCE_REQUEST (TYPE 0x01)
    // Format DATA: <RealmName>&<SigilName>&<FileSize>&<MD5SUM>
    // Per F2: posem valors placeholder per FileSize i MD5SUM (no enviem fitxer)
    char myOrigin[IP_SIZE];
    snprintf(myOrigin, IP_SIZE, "%s:%d", maester->ip, maester->port);
    
    char frameData[DATA_MAX_SIZE];
    snprintf(frameData, DATA_MAX_SIZE, "%s&%s&0&00000000000000000000000000000000", 
             maester->name, sigilPath);
    
    Frame requestFrame;
    createFrame(&requestFrame, ALLIANCE_REQUEST, myOrigin, realmName, frameData);
    
    // 5. Enviar la trama
    if (sendFrame(fd_nextHop, &requestFrame) < 0) {
        asprintf(&msg, RED "ERROR | Failed to send alliance request to [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        close(fd_nextHop);
        return -1;
    }
    
    // 6. Tancar connexió immediatament (NO BLOQUEJANT - no esperem resposta aquí)
    close(fd_nextHop);
    
    // 7. Crear aliança com a PENDING amb timestamp
    addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_PENDING);
    
    // La resposta arribarà pel servidor i serà processada per handleAllianceResponse()
    // El timeout es comprova quan rebem la resposta (si han passat > 120s → FAILED)
    
    return 0;
}



// ═══════════════════════════════════════════════════════════
// STUBS: Funciones pendientes de implementar
// ═══════════════════════════════════════════════════════════

int sendAllianceResponse(Maester *maester, const char *realmName, int accept) {
    if (!maester || !realmName) {
        return -1;
    }
    
    char *msg;
    
    // 1. Buscar l'aliança PENDING amb aquest regne (thread-safe)
    char *savedIp = NULL;
    int savedPort = 0;
    int allianceStatus = ALLIANCE_NONE;
    
    if (!getAllianceInfo(maester, realmName, &savedIp, &savedPort, &allianceStatus, NULL)) {
        asprintf(&msg, RED "ERROR | No pending alliance request from [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
    if (allianceStatus != ALLIANCE_PENDING) {
        asprintf(&msg, RED "ERROR | No pending alliance request from [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        if (savedIp) free(savedIp);
        return -1;
    }
    
    // 2. Verificar que tenim IP (vol dir que hem REBUT la petició, no enviat)
    if (!savedIp || savedPort <= 0) {
        asprintf(&msg, RED "ERROR | You sent this request, wait for their response\n" RESET);
        customWrite(1, msg);
        free(msg);
        if (savedIp) free(savedIp);
        return -1;
    }
    
    // 3. Connectar directament a la IP:Port del sol·licitant
    int fd_dest = -1;
    if (connectToRealmByRoute(savedIp, savedPort, &fd_dest) < 0) {
        asprintf(&msg, RED "ERROR | Cannot connect to [%s] at %s:%d\n" RESET, 
                 realmName, savedIp, savedPort);
        customWrite(1, msg);
        free(msg);
        free(savedIp);
        return -1;
    }
    
    // 4. Crear trama ALLIANCE_RESPONSE (0x03)
    // ORIGIN: IP:Port nostre (per que ell sàpiga com connectar-se directament)
    // DESTINATION: Nom del regne que va demanar l'aliança
    // DATA: ACCEPT/REJECT&<NomNostre>
    char myOrigin[IP_SIZE];
    snprintf(myOrigin, IP_SIZE, "%s:%d", maester->ip, maester->port);
    
    char responseData[DATA_MAX_SIZE];
    snprintf(responseData, DATA_MAX_SIZE, "%s&%s", 
             accept ? "ACCEPT" : "REJECT", maester->name);
    
    Frame responseFrame;
    createFrame(&responseFrame, ALLIANCE_RESPONSE, myOrigin, realmName, responseData);
    
    // 5. Enviar la trama
    if (sendFrame(fd_dest, &responseFrame) < 0) {
        asprintf(&msg, RED "ERROR | Failed to send alliance response\n" RESET);
        customWrite(1, msg);
        free(msg);
        close(fd_dest);
        free(savedIp);
        return -1;
    }
    
    // 6. Esperar ACK (0x31) o NACK (0x69) del sol·licitant
    Frame ackFrame;
    if (receiveFrame(fd_dest, &ackFrame) < 0) {
        asprintf(&msg, YELLOW "WARNING | No ACK received from [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        close(fd_dest);
        // Actualitzem estat igualment (optimista)
        if (accept) {
            addOrUpdateAlliance(maester, realmName, savedIp, savedPort, ALLIANCE_ACTIVE);
        } else {
            addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_NONE);
        }
        free(savedIp);
        return 0;
    }
    
    close(fd_dest);
    
    // 8. Processar resposta i actualitzar estat
    if (ackFrame.type == ACK_FILE) {
        // L'altre ha confirmat recepció a temps
        if (accept) {
            addOrUpdateAlliance(maester, realmName, savedIp, savedPort, ALLIANCE_ACTIVE);
            asprintf(&msg, GREEN "Alliance with %s established.\n" RESET, realmName);
        } else {
            addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_NONE);
            asprintf(&msg, YELLOW "Alliance with %s rejected.\n" RESET, realmName);
        }
        customWrite(1, msg);
        free(msg);
    } else if (ackFrame.type == NACK_ERROR) {
        // L'altre va fer timeout mentre esperava la nostra resposta
        addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_FAILED);
        asprintf(&msg, RED "Alliance with %s failed (timeout on their side).\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
    } else {
        asprintf(&msg, YELLOW "Unexpected response type 0x%02X from [%s]\n" RESET, 
                 ackFrame.type, realmName);
        customWrite(1, msg);
        free(msg);
    }
    
    free(savedIp);
    return 0;
}

int sendProductListRequest(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return -1;
    }
    
    char *msg;
    
    // 1. Buscar ruta al regne aliat (thread-safe)
    // Primero intentamos conexión directa si tenemos IP del aliado
    char *allyIp = NULL;
    int allyPort = 0;
    int allyStatus = ALLIANCE_NONE;
    
    if (!getAllianceInfo(maester, realmName, &allyIp, &allyPort, &allyStatus, NULL)) {
        asprintf(&msg, RED "ERROR | No alliance info for [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
    // 2. Determinar cómo conectar: directo al aliado o via ruta
    char *targetIp = NULL;
    int targetPort = 0;
    
    if (allyIp && allyPort > 0) {
        // Conexión directa al aliado
        targetIp = allyIp;
        targetPort = allyPort;
    } else {
        // Buscar ruta (el aliado responderá desde su IP)
        if (allyIp) free(allyIp);
        if (!getRouteInfo(maester, realmName, &targetIp, &targetPort)) {
            if (!getRouteInfo(maester, NULL, &targetIp, &targetPort)) {
                asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
                customWrite(1, msg);
                free(msg);
                return -1;
            }
        }
    }
    
    // 3. Conectar
    int fd_dest = -1;
    if (connectToRealmByRoute(targetIp, targetPort, &fd_dest) < 0) {
        asprintf(&msg, RED "ERROR | Cannot connect to [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        free(targetIp);
        return -1;
    }
    
    // 4. Crear trama PRODUCT_LIST_REQUEST (TYPE 0x11)
    char myOrigin[IP_SIZE];
    snprintf(myOrigin, IP_SIZE, "%s:%d", maester->ip, maester->port);
    
    Frame requestFrame;
    createFrame(&requestFrame, PRODUCT_LIST_REQUEST, myOrigin, realmName, maester->name);
    
    // 5. Enviar la trama
    if (sendFrame(fd_dest, &requestFrame) < 0) {
        asprintf(&msg, RED "ERROR | Failed to send product list request\n" RESET);
        customWrite(1, msg);
        free(msg);
        close(fd_dest);
        free(targetIp);
        return -1;
    }
    
    // 6. Esperar respuesta (Fase 2: solo ACK)
    Frame responseFrame;
    if (receiveFrame(fd_dest, &responseFrame) < 0) {
        asprintf(&msg, RED "Error [NO_RESPONSE]\n" RESET);
        customWrite(1, msg);
        free(msg);
        close(fd_dest);
        free(targetIp);
        return -1;
    }
    
    close(fd_dest);
    free(targetIp);
    
    // 7. Procesar respuesta
    if (responseFrame.type == ACK_FILE) {
        asprintf(&msg, GREEN "Product list request acknowledged by [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        // Fase 3: aquí se procesaría la lista de productos
        customWrite(1, YELLOW "(Product list processing will be available in Phase 3)\n" RESET);
        return 0;
    } else if (responseFrame.type == ERR_UNAUTHORIZED) {
        asprintf(&msg, RED "Error [UNAUTHORIZED]\n" RESET);
        customWrite(1, msg);
        free(msg);
        return -1;
    } else if (responseFrame.type == NACK_ERROR) {
        asprintf(&msg, RED "Error [NACK]\n" RESET);
        customWrite(1, msg);
        free(msg);
        return -1;
    } else {
        asprintf(&msg, YELLOW "Unexpected response type 0x%02X\n" RESET, responseFrame.type);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
}


void notifyDisconnect(Maester *maester) {
    if (!maester) {
        return;
    }
    
    char *msg;
    
    // Iterar per tots els aliats actius i notificar-los
    pthread_mutex_lock(&maester->alliances_mutex);
    
    for (int i = 0; i < maester->numAlliances; i++) {
        // Només notificar als aliats ACTIUS amb IP:Port vàlids
        if (maester->alliances[i].status == ALLIANCE_ACTIVE && 
            maester->alliances[i].ip != NULL && 
            maester->alliances[i].port > 0) {
            
            // Intentar connectar directament a l'aliat
            int fd = -1;
            if (connectToRealmByRoute(maester->alliances[i].ip, maester->alliances[i].port, &fd) == 0) {

                char myOrigin[IP_SIZE];
                snprintf(myOrigin, IP_SIZE, "%s:%d", maester->ip, maester->port);
                
                Frame disconnectFrame;
                // DATA conté el nom del regne que es desconnecta (nosaltres)
                createFrame(&disconnectFrame, MAESTER_DISCONNECT, myOrigin, maester->alliances[i].name, maester->name);
                
                // Enviar la trama (no esperem ACK)
                if (sendFrame(fd, &disconnectFrame) == 0) {
                    asprintf(&msg, CYAN "Notified [%s] of disconnect\n" RESET, maester->alliances[i].name);
                    customWrite(1, msg);
                    free(msg);
                }
                
                close(fd);
            } else {
            }
        }
    }
    
    pthread_mutex_unlock(&maester->alliances_mutex);
}