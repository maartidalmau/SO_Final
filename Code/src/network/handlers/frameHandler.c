#include "frameHandler.h"

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

            case PING_PONG:
                handlePingPong(maester, frame, fromSocket);
                break;

            case ORDER_REQUEST_HEADER:
            case ORDER_REQUEST_DATA:
                handleTradeRequest(maester, frame, fromSocket);
                break;

            case ORDER_RESPONSE:
                handleOrderResponse(maester, frame, fromSocket);
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
    
    // El nom del regne que es desconnecta està al DATA
    const char *disconnectedRealm = frame->data;
    
    // Si DATA està buit, intentem buscar per IP:Port
    if (strlen(disconnectedRealm) == 0) {
        disconnectedRealm = frame->ip_origin;
    }

    
    // Actualizar estado de alianzas - buscar per nom o per IP:Port
    pthread_mutex_lock(&maester->alliances_mutex);
    for (int i = 0; i < maester->numAlliances; i++) {
        // Buscar per nom del regne
        if (strcasecmp(maester->alliances[i].name, disconnectedRealm) == 0) {
            maester->alliances[i].status = ALLIANCE_NONE;
            break;
        }
        // Buscar per IP:Port (fallback)
        if (maester->alliances[i].ip != NULL) {
            char allyIpPort[64];
            snprintf(allyIpPort, sizeof(allyIpPort), "%s:%d", 
                     maester->alliances[i].ip, maester->alliances[i].port);
            if (strcmp(allyIpPort, frame->ip_origin) == 0) {
                maester->alliances[i].status = ALLIANCE_NONE;
                break;
            }
        }
    }
    pthread_mutex_unlock(&maester->alliances_mutex);
    
    // Mostrar prompt de nou
    customWrite(1, GREEN "$ " RESET);
}

void handleNack(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    (void)fromSocket;
    
    char *msg;
    asprintf(&msg, RED "NACK from realm [%s]\n" RESET, frame->data);
    customWrite(1, msg);
    free(msg);
}


void handleAllianceRequest(Maester *maester, Frame *frame, int fromSocket) {
    char *msg;
    (void)fromSocket;  // Per F2 no enviem ACK aquí
    
    // 1. Parsejar DATA: <RealmName>&<SigilName>&<FileSize>&<MD5SUM>
    char requesterName[64];
    char sigilName[64];
    int fileSize = 0;
    char md5sum[64];
    
    // Mínim necessitem RealmName i SigilName
    if (sscanf(frame->data, "%63[^&]&%63[^&]&%d&%63s", 
               requesterName, sigilName, &fileSize, md5sum) < 2) {
        asprintf(&msg, RED "ERROR | Invalid alliance request format\n" RESET);
        customWrite(1, msg);
        free(msg);
        sendNack(fromSocket, maester->name, "INVALID_FORMAT");
        return;
    }
    
    // 2. Parsejar ORIGIN per obtenir IP:Port del sol·licitant
    char requesterIp[32];
    int requesterPort = 0;
    
    if (sscanf(frame->ip_origin, "%31[^:]:%d", requesterIp, &requesterPort) != 2) {
        asprintf(&msg, RED "ERROR | Invalid origin format in alliance request\n" RESET);
        customWrite(1, msg);
        free(msg);
        sendNack(fromSocket, maester->name, "INVALID_ORIGIN");
        return;
    }
    
    // 3. Verificar si ja tenim aliança amb aquest regne (thread-safe)
    int existingStatus = ALLIANCE_NONE;
    if (getAllianceInfo(maester, requesterName, NULL, NULL, &existingStatus, NULL)) {
        if (existingStatus == ALLIANCE_ACTIVE) {
            asprintf(&msg, YELLOW "Already allied with [%s]\n" RESET, requesterName);
            customWrite(1, msg);
            free(msg);
            return;
        }
        // Si ja tenim PENDING nostre, ara rebem la seva petició - actualitzem IP
    }
    
    // 4. Crear/actualitzar aliança com PENDING amb la IP del sol·licitant
    // Guardem IP:Port per poder respondre directament després (PLEDGE RESPOND)
    addOrUpdateAlliance(maester, requesterName, requesterIp, requesterPort, ALLIANCE_PENDING);
    
    // 5. Mostrar missatge a l'usuari
    asprintf(&msg, MAGENTA "\n>>> Alliance request received from %s.\n" RESET, requesterName);
    customWrite(1, msg);
    free(msg);
    
    // Mostrar prompt de nou
    customWrite(1, GREEN "$ " RESET);
    
    // Per F2: no enviem ACK aquí (no hi ha transferència de sigil)
    // L'usuari ha de fer PLEDGE RESPOND <realm> ACCEPT/REJECT
}

void handleSigilSend(Maester *maester, Frame *frame, int fromSocket) {
    char *msg;

    if (!maester || !frame) {
        return;
    }

    char requesterName[64];
    requesterName[0] = '\0';

    pthread_mutex_lock(&maester->alliances_mutex);
    for (int i = 0; i < maester->numAlliances; i++) {
        if (maester->alliances[i].ip && maester->alliances[i].port > 0) {
            char allyIpPort[IP_SIZE];
            snprintf(allyIpPort, sizeof(allyIpPort), "%s:%d",
                     maester->alliances[i].ip, maester->alliances[i].port);
            if (strcmp(allyIpPort, frame->ip_origin) == 0) {
                strncpy(requesterName, maester->alliances[i].name, sizeof(requesterName) - 1);
                break;
            }
        }
    }
    pthread_mutex_unlock(&maester->alliances_mutex);

    if (requesterName[0] == '\0') {
        sendNack(fromSocket, maester->name, "UNKNOWN_REQUESTER");
        return;
    }

    char sigilPath[512];
    snprintf(sigilPath, sizeof(sigilPath), "Assets/%s.png", requesterName);

    int fd = open(sigilPath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        asprintf(&msg, RED "ERROR | Cannot create sigil file [%s]\n" RESET, sigilPath);
        customWrite(1, msg);
        free(msg);
        sendNack(fromSocket, maester->name, "FILE_ERROR");
        return;
    }

    uint16_t dataLen = ntohs(frame->data_length);
    if (write(fd, frame->data, dataLen) < 0) {
        close(fd);
        asprintf(&msg, RED "ERROR | Failed to write sigil data\n" RESET);
        customWrite(1, msg);
        free(msg);
        sendNack(fromSocket, maester->name, "WRITE_ERROR");
        return;
    }

    close(fd);

    int isLastChunk = (dataLen < DATA_MAX_SIZE);
    if (isLastChunk) {
        asprintf(&msg, CYAN ">>> Sigil received from [%s], saved to [%s]\n" RESET,
                 requesterName, sigilPath);
        customWrite(1, msg);
        free(msg);
    }

    Frame ackFrame;
    createFrame(&ackFrame, ACK_FILE, "", "", "OK");
    sendFrame(fromSocket, &ackFrame);
}

void handleAllianceResponse(Maester *maester, Frame *frame, int fromSocket) {
    char *msg;
    
    // Parsejar DATA: ACCEPT&<RealmName> o REJECT&<RealmName>
    char responseType[16];
    char responderName[64];
    
    if (sscanf(frame->data, "%15[^&]&%63s", responseType, responderName) != 2) {
        asprintf(&msg, RED "ERROR | Invalid alliance response format\n" RESET);
        customWrite(1, msg);
        free(msg);
        sendNack(fromSocket, maester->name, "INVALID_FORMAT");
        return;
    }
    
    // Parsejar ORIGIN per obtenir IP:Port de l'aliat
    char responderIp[32];
    int responderPort = 0;
    
    if (sscanf(frame->ip_origin, "%31[^:]:%d", responderIp, &responderPort) != 2) {
        responderIp[0] = '\0';
        responderPort = 0;
    }
    
    // Buscar l'aliança PENDING per comprovar el timeout (thread-safe)
    int allianceStatus = ALLIANCE_NONE;
    time_t requestTime = 0;
    
    if (!getAllianceInfo(maester, responderName, NULL, NULL, &allianceStatus, &requestTime) ||
        allianceStatus != ALLIANCE_PENDING) {
        asprintf(&msg, YELLOW "Received response from [%s] but no pending request found\n" RESET, responderName);
        customWrite(1, msg);
        free(msg);
        return;
    }
    
    // COMPROVAR TIMEOUT: si han passat més de 120 segons
    time_t now = time(NULL);
    double elapsed = difftime(now, requestTime);
    
    if (elapsed > ALLIANCE_TIMEOUT_SECONDS) {
        // TIMEOUT - enviar NACK i marcar com FAILED
        asprintf(&msg, RED "\n>>> Pledge to %s has failed (TIMEOUT - response arrived after %.0f seconds).\n" RESET, 
                 responderName, elapsed);
        customWrite(1, msg);
        free(msg);
        
        // Actualitzar estat
        addOrUpdateAlliance(maester, responderName, NULL, 0, ALLIANCE_FAILED);
        releaseEnvoyMissionForRealm(maester, responderName, "PLEDGE to ");
        
        // Enviar NACK (0x69) al que ens ha respost
        Frame nackFrame;
        createFrame(&nackFrame, NACK_ERROR, "", "", maester->name);
        sendFrame(fromSocket, &nackFrame);
        
        customWrite(1, GREEN "$ " RESET);
        return;
    }
    
    // RESPOSTA A TEMPS - Enviar ACK (0x31) confirmant recepció
    Frame ackFrame;
    char ackData[DATA_MAX_SIZE];
    snprintf(ackData, DATA_MAX_SIZE, "OK&%s", maester->name);
    createFrame(&ackFrame, ACK_FILE, "", "", ackData);
    sendFrame(fromSocket, &ackFrame);
    
    // Actualitzar estat de l'aliança segons resposta
    if (strcasecmp(responseType, "ACCEPT") == 0) {
        // Aliança acceptada! Guardar IP:Port per connexió directa futura
        addOrUpdateAlliance(maester, responderName, responderIp, responderPort, ALLIANCE_ACTIVE);

        // Enviar sigil al realm que va acceptar
        char *sigilPath = getAllianceSigil(maester, responderName);
        if (sigilPath) {
            int envoyIndex = reserveEnvoy(maester);
            if (envoyIndex >= 0) {
                IpcRequest sigilRequest;
                IpcResponse sigilResponse;
                memset(&sigilRequest, 0, sizeof(IpcRequest));

                sigilRequest.type = IPC_SEND_SIGIL;
                strncpy(sigilRequest.source_realm, maester->name, IPC_REALM_SIZE - 1);
                strncpy(sigilRequest.source_ip, maester->ip, IPC_IP_SIZE - 1);
                sigilRequest.source_port = (uint32_t)maester->port;
                strncpy(sigilRequest.target_realm, responderName, IPC_REALM_SIZE - 1);
                strncpy(sigilRequest.target_ip, responderIp, IPC_IP_SIZE - 1);
                sigilRequest.target_port = (uint32_t)responderPort;
                strncpy(sigilRequest.path, sigilPath, IPC_PATH_SIZE - 1);

                asprintf(&msg, "PLEDGE SIGIL to %s", responderName);
                setEnvoyMission(maester, envoyIndex, msg);
                free(msg);

                if (dispatchEnvoyRequest(maester, envoyIndex, &sigilRequest, &sigilResponse) < 0) {
                    asprintf(&msg, YELLOW "Warning: Failed to send sigil to [%s]\n" RESET, responderName);
                    customWrite(1, msg);
                    free(msg);
                }

                releaseEnvoy(maester, envoyIndex);
            }
            free(sigilPath);
        }

        releaseEnvoyMissionForRealm(maester, responderName, "PLEDGE to ");

        asprintf(&msg, GREEN "\n>>> Alliance with %s forged successfully!\n" RESET, responderName);
        customWrite(1, msg);
        free(msg);
    } else {
        // Aliança rebutjada
        addOrUpdateAlliance(maester, responderName, NULL, 0, ALLIANCE_FAILED);
        releaseEnvoyMissionForRealm(maester, responderName, "PLEDGE to ");
        
        asprintf(&msg, YELLOW "\n>>> Alliance with %s was rejected.\n" RESET, responderName);
        customWrite(1, msg);
        free(msg);
    }
    
    // Mostrar prompt de nou
    customWrite(1, GREEN "$ " RESET);
}

// ═══════════════════════════════════════════════════════════
// HANDLERS DE PRODUCTOS
// ═══════════════════════════════════════════════════════════
void handleProductListRequest(Maester *maester, Frame *frame, int fromSocket) {
    char *msg;
    
    // El nombre del reino solicitante viene en DATA (enviado por sendProductListRequest)
    // frame->ip_origin contiene IP:Port, no el nombre
    const char *requesterName = frame->data;
    
    asprintf(&msg, CYAN "Product list request from [%s]\n" RESET, requesterName);
    customWrite(1, msg);
    free(msg);
    
    // Verificar si tenim aliança amb aquest regne (por nombre, no por IP)
    if (!hasAlliance(maester, requesterName)) {
        // No tenim aliança - enviar ERR_UNAUTHORIZED (0x25)
        asprintf(&msg, RED "No alliance with [%s] - sending UNAUTHORIZED\n" RESET, requesterName);
        customWrite(1, msg);
        free(msg);
        
        char myIpPort[IP_SIZE];
        snprintf(myIpPort, IP_SIZE, "%s:%d", maester->ip, maester->port);
        
        char errorData[DATA_MAX_SIZE];
        snprintf(errorData, DATA_MAX_SIZE, "AUTH&%.200s", requesterName);
        
        Frame errorFrame;
        createFrame(&errorFrame, ERR_UNAUTHORIZED, myIpPort, frame->ip_origin, errorData);
        
        sendFrame(fromSocket, &errorFrame);
        return;
    }
    
    // Construïm un catàleg compacte per a poder reutilitzar-lo a START TRADE.
    char catalog[DATA_MAX_SIZE];
    int written = snprintf(catalog, sizeof(catalog), "OK&%s&", maester->name);
    if (written < 0 || written >= (int)sizeof(catalog)) {
        catalog[0] = '\0';
    }
    for (int i = 0; i < maester->numProducts && written > 0 && written < (int)sizeof(catalog) - 1; i++) {
        int added = snprintf(catalog + written, sizeof(catalog) - (size_t)written, "%s%s",
                             maester->inventory[i].name,
                             (i < maester->numProducts - 1) ? "|" : "");
        if (added < 0 || written + added >= (int)sizeof(catalog)) {
            break;
        }
        written += added;
    }

    asprintf(&msg, GREEN "Alliance verified with [%s] - sending cached catalog ACK\n" RESET, requesterName);
    customWrite(1, msg);
    free(msg);
    
    Frame ackFrame;
    createFrame(&ackFrame, ACK_FILE, "", "", catalog);
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

void handlePingPong(Maester *maester, Frame *frame, int fromSocket) {
    (void)maester;
    char *msg;

    if (!frame) {
        return;
    }

    // Determinar si es PING o PONG
    int isPing = (strncmp(frame->data, "PING", 4) == 0);
    int isPong = (strncmp(frame->data, "PONG", 4) == 0);

    if (isPing) {
        // PING received
        asprintf(&msg, CYAN ">>> Received PING from [%s]\n" RESET, frame->ip_origin);
        customWrite(1, msg);
        free(msg);

        // Create PONG
        Frame pongFrame;
        createFrame(&pongFrame, PING_PONG, frame->ip_destination, frame->ip_origin, "PONG");

        // Send PONG response
        if (sendFrame(fromSocket, &pongFrame) < 0) {
            asprintf(&msg, RED "ERROR | Failed to send PONG response\n" RESET);
            customWrite(1, msg);
            free(msg);
        } else {
            asprintf(&msg, CYAN ">>> Sent PONG to [%s]\n" RESET, frame->ip_origin);
            customWrite(1, msg);
            free(msg);
        }
    } else if (isPong) {
        // PONG received
        asprintf(&msg, CYAN ">>> Received PONG from [%s]\n" RESET, frame->ip_origin);
        customWrite(1, msg);
        free(msg);
    } else {
        // Invalid data
        asprintf(&msg, RED "ERROR | Invalid PING/PONG data: %s\n" RESET, frame->data);
        customWrite(1, msg);
        free(msg);
    }
}

void handleTradeRequest(Maester *maester, Frame *frame, int fromSocket) {
    char *msg;
    static char tradeFilePath[512] = "";
    static long totalTradeSize = 0;
    static long receivedBytes = 0;

    if (!maester || !frame) {
        return;
    }

    char requesterName[64];
    requesterName[0] = '\0';

    pthread_mutex_lock(&maester->alliances_mutex);
    for (int i = 0; i < maester->numAlliances; i++) {
        if (maester->alliances[i].ip && maester->alliances[i].port > 0) {
            char allyIpPort[IP_SIZE];
            snprintf(allyIpPort, sizeof(allyIpPort), "%s:%d",
                     maester->alliances[i].ip, maester->alliances[i].port);
            if (strcmp(allyIpPort, frame->ip_origin) == 0) {
                strncpy(requesterName, maester->alliances[i].name, sizeof(requesterName) - 1);
                break;
            }
        }
    }
    pthread_mutex_unlock(&maester->alliances_mutex);

    if (requesterName[0] == '\0') {
        sendNack(fromSocket, maester->name, "UNKNOWN_REQUESTER");
        return;
    }

    if (frame->type == ORDER_REQUEST_HEADER) {
        totalTradeSize = atol(frame->data);
        receivedBytes = 0;

        snprintf(tradeFilePath, sizeof(tradeFilePath), "/tmp/trade_%s_%ld.tmp",
                 requesterName, time(NULL));

        int fd = open(tradeFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            sendNack(fromSocket, maester->name, "FILE_ERROR");
            return;
        }
        close(fd);

        asprintf(&msg, CYAN ">>> Trade request from [%s], total size: %ld bytes\n" RESET,
                 requesterName, totalTradeSize);
        customWrite(1, msg);
        free(msg);

        Frame ackFrame;
        createFrame(&ackFrame, ACK_FILE, "", "", "OK");
        sendFrame(fromSocket, &ackFrame);

    } else if (frame->type == ORDER_REQUEST_DATA) {
        uint16_t dataLen = ntohs(frame->data_length);
        receivedBytes += dataLen;

        int fd = open(tradeFilePath, O_WRONLY | O_APPEND);
        if (fd < 0) {
            sendNack(fromSocket, maester->name, "FILE_ERROR");
            return;
        }

        if (write(fd, frame->data, dataLen) < 0) {
            close(fd);
            sendNack(fromSocket, maester->name, "WRITE_ERROR");
            return;
        }
        close(fd);

        int isComplete = (dataLen < DATA_MAX_SIZE);
        if (isComplete || receivedBytes >= totalTradeSize) {
            asprintf(&msg, CYAN ">>> Trade received from [%s], total: %ld bytes\n" RESET,
                     requesterName, receivedBytes);
            customWrite(1, msg);
            free(msg);

            char orderResponseData[DATA_MAX_SIZE];
            int response_ok = 1;

            int fd_trade = open(tradeFilePath, O_RDONLY);
            if (fd_trade < 0) {
                snprintf(orderResponseData, DATA_MAX_SIZE, "REJECT|Cannot read trade file");
                response_ok = 0;
            } else {
                char *line = NULL;
                size_t lineLen = 0;

                FILE *fp = fdopen(fd_trade, "r");
                if (fp) {
                    ssize_t readLen;
                    while ((readLen = getline(&line, &lineLen, fp)) != -1) {
                        int qty = 0;
                        char product[128];

                        if (sscanf(line, "%d x %127s", &qty, product) == 2) {
                            if (decrementInventory(maester, product, qty) < 0) {
                                snprintf(orderResponseData, DATA_MAX_SIZE,
                                        "REJECT|Insufficient stock for %s", product);
                                response_ok = 0;
                                break;
                            }
                        }
                    }
                    free(line);
                    fclose(fp);

                    if (response_ok) {
                        updateStockDB(maester->path, maester);
                        snprintf(orderResponseData, DATA_MAX_SIZE, "OK|Trade accepted");
                    }
                } else {
                    snprintf(orderResponseData, DATA_MAX_SIZE, "REJECT|Cannot parse trade");
                    response_ok = 0;
                }
            }

            Frame responseFrame;
            createFrame(&responseFrame, ORDER_RESPONSE, "", "", orderResponseData);
            sendFrame(fromSocket, &responseFrame);

            unlink(tradeFilePath);
            tradeFilePath[0] = '\0';
            totalTradeSize = 0;
            receivedBytes = 0;
        } else {
            Frame ackFrame;
            createFrame(&ackFrame, ACK_FILE, "", "", "OK");
            sendFrame(fromSocket, &ackFrame);
        }
    }
}

void handleOrderResponse(Maester *maester, Frame *frame, int fromSocket) {
    (void)fromSocket;
    char *msg;

    if (!maester || !frame) {
        return;
    }

    char responseType[16];
    char responseDetail[256];

    if (sscanf(frame->data, "%15[^|]|%255s", responseType, responseDetail) < 1) {
        asprintf(&msg, RED "ERROR | Invalid order response format\n" RESET);
        customWrite(1, msg);
        free(msg);
        return;
    }

    if (strcasecmp(responseType, "OK") == 0) {
        asprintf(&msg, GREEN "\n>>> Trade ACCEPTED by [%s]\n" RESET, frame->ip_origin);
        customWrite(1, msg);
        free(msg);
        customWrite(1, GREEN "$ " RESET);
    } else {
        asprintf(&msg, RED "\n>>> Trade REJECTED: %s\n" RESET, responseDetail);
        customWrite(1, msg);
        free(msg);
        customWrite(1, GREEN "$ " RESET);
    }
}
