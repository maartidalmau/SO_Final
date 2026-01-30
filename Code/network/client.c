#include "client.h"


int sendAllianceRequest(Maester *maester, const char *realmName, const char *sigilPath) {
    if (!maester || !realmName || !sigilPath) {
        return -1;
    }
    char *msg;
    
    // 1. Verificar que no tenim ja aliança activa amb aquest regne
    Alliance *existing = findAlliance(maester, realmName);
    if (existing) {
        if (existing->status == ALLIANCE_ACTIVE) {
            asprintf(&msg, YELLOW "Already allied with [%s]\n" RESET, realmName);
            customWrite(1, msg);
            free(msg);
            return 0;
        }
        if (existing->status == ALLIANCE_PENDING) {
            asprintf(&msg, YELLOW "Alliance request to [%s] already pending\n" RESET, realmName);
            customWrite(1, msg);
            free(msg);
            return 0;
        }
    }
    
    // 2. Buscar ruta al regne (directa o via DEFAULT)
    Route *route = findRoute(maester, realmName);
    
    if (!route || strcasecmp(route->ip, "*.*.*.*") == 0) {
        route = getDefaultRoute(maester);
    }
    
    if (!route) {
        asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
    // 3. Connectar al següent hop
    int raven_fd = -1;
    if (connectToRealm(route, &raven_fd) < 0) {
        asprintf(&msg, RED "ERROR | Cannot connect to route for [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
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
    if (sendFrame(raven_fd, &requestFrame) < 0) {
        asprintf(&msg, RED "ERROR | Failed to send alliance request to [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        close(raven_fd);
        return -1;
    }
    
    // 6. Tancar connexió immediatament (NO BLOQUEJANT - no esperem resposta aquí)
    close(raven_fd);
    
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
    
    // 1. Buscar l'aliança PENDING amb aquest regne
    Alliance *alliance = findAlliance(maester, realmName);
    
    if (!alliance || alliance->status != ALLIANCE_PENDING) {
        asprintf(&msg, RED "ERROR | No pending alliance request from [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
    // 2. Verificar que tenim IP (vol dir que hem REBUT la petició, no enviat)
    if (!alliance->ip || alliance->port <= 0) {
        asprintf(&msg, RED "ERROR | You sent this request, wait for their response\n" RESET);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
    // 3. IMPORTANT: Guardar còpies de IP i port ABANS de qualsevol operació
    //    perquè el punter alliance pot quedar invàlid després de addOrUpdateAlliance
    char *savedIp = strdup(alliance->ip);
    int savedPort = alliance->port;
    
    if (!savedIp) {
        customWrite(1, RED "ERROR | Memory allocation failed\n" RESET);
        return -1;
    }
    
    // 4. Connectar directament a la IP:Port del sol·licitant
    int raven_fd = -1;
    if (connectToRealmByRoute(savedIp, savedPort, &raven_fd) < 0) {
        asprintf(&msg, RED "ERROR | Cannot connect to [%s] at %s:%d\n" RESET, 
                 realmName, savedIp, savedPort);
        customWrite(1, msg);
        free(msg);
        free(savedIp);
        return -1;
    }
    
    // 5. Crear trama ALLIANCE_RESPONSE (0x03)
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
    
    // 6. Enviar la trama
    if (sendFrame(raven_fd, &responseFrame) < 0) {
        asprintf(&msg, RED "ERROR | Failed to send alliance response\n" RESET);
        customWrite(1, msg);
        free(msg);
        close(raven_fd);
        free(savedIp);
        return -1;
    }
    
    // 7. Esperar ACK (0x31) o NACK (0x69) del sol·licitant
    Frame ackFrame;
    if (receiveFrame(raven_fd, &ackFrame) < 0) {
        asprintf(&msg, YELLOW "WARNING | No ACK received from [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        close(raven_fd);
        // Actualitzem estat igualment (optimista)
        if (accept) {
            addOrUpdateAlliance(maester, realmName, savedIp, savedPort, ALLIANCE_ACTIVE);
        } else {
            addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_NONE);
        }
        free(savedIp);
        return 0;
    }
    
    close(raven_fd);
    
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
    (void)maester;
    (void)realmName;
    
    customWrite(1, YELLOW "TODO: sendProductListRequest not yet implemented\n" RESET);
    return -1;
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
            int raven_fd = -1;
            if (connectToRealmByRoute(maester->alliances[i].ip, maester->alliances[i].port, &raven_fd) == 0) {

                char myOrigin[IP_SIZE];
                snprintf(myOrigin, IP_SIZE, "%s:%d", maester->ip, maester->port);
                
                Frame disconnectFrame;
                // DATA conté el nom del regne que es desconnecta (nosaltres)
                createFrame(&disconnectFrame, MAESTER_DISCONNECT, myOrigin, maester->alliances[i].name, maester->name);
                
                // Enviar la trama (no esperem ACK)
                if (sendFrame(raven_fd, &disconnectFrame) == 0) {
                    asprintf(&msg, CYAN "Notified [%s] of disconnect\n" RESET, maester->alliances[i].name);
                    customWrite(1, msg);
                    free(msg);
                }
                
                close(raven_fd);
            } else {
            }
        }
    }
    
    pthread_mutex_unlock(&maester->alliances_mutex);
}