#include "frameHandler.h"
#include "md5.h"

static void mkdirRecursive(const char *path) {
    if (!path || !*path) {
        return;
    }
    char tmp[512];
    size_t n = strlen(path);
    if (n >= sizeof(tmp)) {
        n = sizeof(tmp) - 1;
    }
    memcpy(tmp, path, n);
    tmp[n] = '\0';
    if (n > 0 && tmp[n - 1] == '/') {
        tmp[n - 1] = '\0';
    }
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);  
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

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

            case ALLIANCE_REQUEST:
                handleAllianceRequest(maester, frame, fromSocket);
                break;

            case ALLIANCE_RESPONSE:
                handleAllianceResponse(maester, frame, fromSocket);
                break;

            case PRODUCT_LIST_REQUEST:
                handleProductListRequest(maester, frame, fromSocket);
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

void handleAllianceRequest(Maester *maester, Frame *frame, int fromSocket) {
    char *msg;

    // 1. Parsejar DATA: <RealmName>&<SigilName>&<FileSize>&<MD5SUM>
    char requesterName[64] = "";
    char sigilName[64] = "";
    long fileSize = 0;
    char expectedMd5[64] = "";

    if (sscanf(frame->data, "%63[^&]&%63[^&]&%ld&%63s",
               requesterName, sigilName, &fileSize, expectedMd5) < 4) {
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

    // 3. Guardar aliança PENDING amb la IP del sol·licitant (per respondre després)
    addOrUpdateAlliance(maester, requesterName, requesterIp, requesterPort, ALLIANCE_PENDING);

    // 4. ACK FITXER (0x31): confirmem que estem llestos per rebre el segell
    char ackData[DATA_MAX_SIZE];
    snprintf(ackData, DATA_MAX_SIZE, "OK&%s", maester->name);
    Frame ackFrame;
    createFrame(&ackFrame, ACK_FILE, "", "", ackData);
    if (sendFrame(fromSocket, &ackFrame) < 0) {
        return;
    }

    // 5. Rebre el segell (0x02) en un buffer en MEMÒRIA (sense temp file)
    uint8_t *sigBuf = NULL;
    if (fileSize > 0) {
        sigBuf = malloc((size_t)fileSize);
        if (!sigBuf) {
            sendNack(fromSocket, maester->name, "MEM_ERROR");
            return;
        }
    }
    long received = 0;
    int recvErr = 0;
    while (received < fileSize) {
        Frame dataFrame;
        if (receiveFrame(fromSocket, &dataFrame) < 0 || dataFrame.type != SIGIL_SEND) {
            recvErr = 1;
            break;
        }
        uint16_t len = dataFrame.data_length;
        if (len > 0 && received + (long)len <= fileSize) {
            memcpy(sigBuf + received, dataFrame.data, len);
        }
        received += len;
    }

    // 6. Verificar md5 (en memòria) i respondre ACK MD5SUM (0x32)
    int md5ok = 0;
    if (!recvErr) {
        char *actualMd5 = md5_buffer(sigBuf, (size_t)fileSize);
        md5ok = (actualMd5 && strcmp(actualMd5, expectedMd5) == 0);
        free(actualMd5);
    }

    char checkData[DATA_MAX_SIZE];
    snprintf(checkData, DATA_MAX_SIZE, "%s&%s",
             md5ok ? "CHECK_OK" : "CHECK_KO", maester->name);
    Frame checkFrame;
    createFrame(&checkFrame, ACK_MD5SUM, "", "", checkData);
    sendFrame(fromSocket, &checkFrame);

    if (!md5ok) {
        free(sigBuf);
        // Segell corromput: descartem la petició pendent
        addOrUpdateAlliance(maester, requesterName, NULL, 0, ALLIANCE_NONE);
        asprintf(&msg, RED "\n>>> Alliance request from %s discarded (sigil corrupted).\n" RESET, requesterName);
        customWrite(1, msg);
        free(msg);
        customWrite(1, GREEN "$ " RESET);
        return;
    }

    // Segell verificat: el desem a la carpeta de l'usuari (ruta relativa "/a"->"./a").
    const char *folder = (maester->path && maester->path[0]) ? maester->path : ".";
    while (*folder == '/') {
        folder++;
    }
    if (*folder == '\0') {
        folder = ".";
    }
    mkdirRecursive(folder);

    char sigilPath[512];
    snprintf(sigilPath, sizeof(sigilPath), "%s/%s.png", folder, requesterName);
    int fd = open(sigilPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        if (sigBuf && fileSize > 0) {
            ssize_t w = write(fd, sigBuf, (size_t)fileSize);
            (void)w;  // el segell ja s'ha verificat; desar-lo és best-effort
        }
        close(fd);
    } else {
        asprintf(&msg, YELLOW "Warning: cannot save sigil to [%s]\n" RESET, sigilPath);
        customWrite(1, msg);
        free(msg);
    }
    free(sigBuf);

    // 7. Segell OK: avisem l'usuari, que decidirà amb PLEDGE RESPOND
    asprintf(&msg, MAGENTA "\n>>> Alliance request received from %s (sigil verified).\n" RESET, requesterName);
    customWrite(1, msg);
    free(msg);
    customWrite(1, GREEN "$ " RESET);
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

        // El segell ja s'ha transferit i verificat durant la petició (0x01/0x02),
        // per tant aquí només cal confirmar l'aliança.
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

    // El nom del regne sol·licitant ve al DATA (ip_origin porta IP:Port, no el nom)
    const char *requesterName = frame->data;

    asprintf(&msg, CYAN ">>> LIST PRODUCTS request from [%s].\n" RESET, requesterName);
    customWrite(1, msg);
    free(msg);

    char myIpPort[IP_SIZE];
    snprintf(myIpPort, IP_SIZE, "%s:%d", maester->ip, maester->port);

    // Sense aliança -> ERR_UNAUTHORIZED (0x25)
    if (!hasAlliance(maester, requesterName)) {
        char errorData[DATA_MAX_SIZE];
        snprintf(errorData, DATA_MAX_SIZE, "AUTH&%.200s", requesterName);
        Frame errorFrame;
        createFrame(&errorFrame, ERR_UNAUTHORIZED, myIpPort, frame->ip_origin, errorData);
        sendFrame(fromSocket, &errorFrame);
        return;
    }

    // 1. Serialitzem el nostre inventari a un buffer en MEMÒRIA (sense temp file)
    long fileSize = (long)maester->numProducts * (long)sizeof(AuxiliarProduct);
    uint8_t *invBuf = NULL;
    if (fileSize > 0) {
        invBuf = malloc((size_t)fileSize);
        if (!invBuf) {
            sendNack(fromSocket, maester->name, "MEM_ERROR");
            return;
        }
        pthread_mutex_lock(&maester->inventory_mutex);
        for (int i = 0; i < maester->numProducts; i++) {
            AuxiliarProduct aux;
            memset(&aux, 0, sizeof(aux));
            strncpy(aux.name, maester->inventory[i].name, sizeof(aux.name) - 1);
            aux.amount = maester->inventory[i].amount;
            aux.weight = maester->inventory[i].weight;
            memcpy(invBuf + (size_t)i * sizeof(AuxiliarProduct), &aux, sizeof(AuxiliarProduct));
        }
        pthread_mutex_unlock(&maester->inventory_mutex);
    }

    char *md5 = md5_buffer(invBuf, (size_t)fileSize);
    if (!md5) {
        free(invBuf);
        sendNack(fromSocket, maester->name, "MD5_ERROR");
        return;
    }

    // 2. Trama HEADER (0x12): FileName&FileSize&MD5SUM
    char headerData[DATA_MAX_SIZE];
    snprintf(headerData, DATA_MAX_SIZE, "products.db&%ld&%s", fileSize, md5);
    free(md5);

    Frame headerFrame;
    createFrame(&headerFrame, PRODUCT_LIST_HEADER, myIpPort, requesterName, headerData);
    if (sendFrame(fromSocket, &headerFrame) < 0) {
        free(invBuf);
        return;
    }

    // 3. Esperem ACK FITXER (0x31) conforme el sol·licitant està llest
    Frame ackFrame;
    if (receiveFrame(fromSocket, &ackFrame) < 0 ||
        ackFrame.type != ACK_FILE || strncmp(ackFrame.data, "OK", 2) != 0) {
        free(invBuf);
        return;
    }

    // 4. Enviem el buffer en blocs binaris (0x13)
    Frame dataFrame;
    long sent = 0;
    while (sent < fileSize) {
        long remaining = fileSize - sent;
        uint16_t chunkLen = (remaining > DATA_MAX_SIZE) ? DATA_MAX_SIZE : (uint16_t)remaining;
        createBinaryFrame(&dataFrame, PRODUCT_LIST_DATA, myIpPort, requesterName,
                          invBuf + sent, chunkLen);
        if (sendFrame(fromSocket, &dataFrame) < 0) {
            free(invBuf);
            return;
        }
        sent += chunkLen;
    }
    free(invBuf);

    // 5. Rebem ACK MD5SUM (0x32) amb el resultat de la verificació
    Frame checkFrame;
    if (receiveFrame(fromSocket, &checkFrame) == 0 &&
        checkFrame.type == ACK_MD5SUM && strncmp(checkFrame.data, "CHECK_OK", 8) == 0) {
        asprintf(&msg, GREEN "Products delivered to [%s].\n" RESET, requesterName);
    } else {
        asprintf(&msg, YELLOW "Products sent to [%s] but verification failed.\n" RESET, requesterName);
    }
    customWrite(1, msg);
    free(msg);
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

    if (!maester || !frame) {
        return;
    }

    // El handler pren el control del socket en rebre la capçalera (0x14) i
    // gestiona tot l'intercanvi de la comanda en una sola connexió.
    if (frame->type != ORDER_REQUEST_HEADER) {
        return;
    }

    // 1. Identificar el regne sol·licitant per IP:Port (ha de ser aliat)
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

    // 2. Parsejar capçalera: <FileName>&<FileSize>&<MD5SUM>
    char fileName[128] = "";
    long fileSize = 0;
    char expectedMd5[64] = "";
    if (sscanf(frame->data, "%127[^&]&%ld&%63s", fileName, &fileSize, expectedMd5) < 3) {
        sendNack(fromSocket, maester->name, "INVALID_FORMAT");
        return;
    }

    asprintf(&msg, CYAN ">>> Trade request from [%s] (%ld bytes).\n" RESET, requesterName, fileSize);
    customWrite(1, msg);
    free(msg);

    // 3. ACK FITXER (0x31)
    char ackData[DATA_MAX_SIZE];
    snprintf(ackData, DATA_MAX_SIZE, "OK&%s", maester->name);
    Frame ackFrame;
    createFrame(&ackFrame, ACK_FILE, "", "", ackData);
    if (sendFrame(fromSocket, &ackFrame) < 0) {
        return;
    }

    // 4. Rebre DADES (0x15) en un buffer en MEMÒRIA (sense temp file)
    char *content = NULL;
    if (fileSize > 0) {
        content = malloc((size_t)fileSize + 1);
        if (!content) {
            sendNack(fromSocket, maester->name, "MEM_ERROR");
            return;
        }
    }
    long received = 0;
    int recvErr = 0;
    while (received < fileSize) {
        Frame dataFrame;
        if (receiveFrame(fromSocket, &dataFrame) < 0 || dataFrame.type != ORDER_REQUEST_DATA) {
            recvErr = 1;
            break;
        }
        uint16_t len = dataFrame.data_length;
        if (len > 0 && received + (long)len <= fileSize) {
            memcpy(content + received, dataFrame.data, len);
        }
        received += len;
    }
    if (content) {
        content[fileSize] = '\0';
    }

    // 5. Verificar md5 (en memòria) i enviar ACK MD5SUM (0x32)
    int md5ok = 0;
    if (!recvErr) {
        char *actualMd5 = md5_buffer((const uint8_t *)content, (size_t)fileSize);
        md5ok = (actualMd5 && strcmp(actualMd5, expectedMd5) == 0);
        free(actualMd5);
    }
    char checkData[DATA_MAX_SIZE];
    snprintf(checkData, DATA_MAX_SIZE, "%s&%s", md5ok ? "CHECK_OK" : "CHECK_KO", maester->name);
    Frame checkFrame;
    createFrame(&checkFrame, ACK_MD5SUM, "", "", checkData);
    sendFrame(fromSocket, &checkFrame);

    // 6. Processar la comanda (parsejant el buffer) i preparar RESPOSTA (0x16)
    char orderResponseData[DATA_MAX_SIZE];
    int response_ok = 1;

    if (!md5ok) {
        snprintf(orderResponseData, DATA_MAX_SIZE, "REJECT&CORRUPTED");
        response_ok = 0;
    } else {
        char *saveptr = NULL;
        char *lineTok = content ? strtok_r(content, "\n", &saveptr) : NULL;
        while (lineTok) {
            int qty = 0;
            char product[128];
            // El producte pot tenir espais: capturem fins al final de línia
            if (sscanf(lineTok, "%d x %127[^\n]", &qty, product) == 2) {
                if (decrementInventory(maester, product, qty) < 0) {
                    snprintf(orderResponseData, DATA_MAX_SIZE, "REJECT&OUT_OF_STOCK");
                    response_ok = 0;
                    break;
                }
            }
            lineTok = strtok_r(NULL, "\n", &saveptr);
        }
        if (response_ok) {
            // Persistim l'inventari al fitxer d'stock real
            updateStockDB(maester->stockFile, maester);
            snprintf(orderResponseData, DATA_MAX_SIZE, "OK");
        }
    }
    free(content);

    // 7. RESPOSTA A COMANDA (0x16)
    Frame responseFrame;
    createFrame(&responseFrame, ORDER_RESPONSE, "", "", orderResponseData);
    sendFrame(fromSocket, &responseFrame);

    if (response_ok) {
        asprintf(&msg, GREEN ">>> Order from [%s] processed. Stock updated.\n" RESET, requesterName);
    } else {
        asprintf(&msg, YELLOW ">>> Order from [%s] rejected.\n" RESET, requesterName);
    }
    customWrite(1, msg);
    free(msg);
    customWrite(1, GREEN "$ " RESET);
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
