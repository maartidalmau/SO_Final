#include "client.h"
#include "envoy.h"
#include "md5.h"
#include "list.h"

#include <time.h>

static int connectForRequest(const IpcRequest *request, int *fd_out) {
    return connectToRealmByRoute(request->target_ip, (int)request->target_port, fd_out);
}

static void fillBasicError(IpcResponse *response, const char *message) {
    response->status = IPC_STATUS_ERROR;
    response->result_code = -1;
    if (message) {
        strncpy(response->payload, message, IPC_PAYLOAD_SIZE - 1);
    }
}


int envoySendAllianceRequest(const IpcRequest *request, IpcResponse *response) {
    int fd_nextHop = -1;
    if (connectForRequest(request, &fd_nextHop) < 0) {
        fillBasicError(response, "Cannot connect to route");
        return -1;
    }

    char origin[IPC_IP_SIZE + 16];
    snprintf(origin, sizeof(origin), "%s:%u", request->source_ip, request->source_port);

    // Llegim el segell SENCER a memòria (sense temp file) i en calculem mida i md5
    int fd_sigil = open(request->path, O_RDONLY);
    if (fd_sigil < 0) {
        close(fd_nextHop);
        fillBasicError(response, "Cannot open sigil file");
        return -1;
    }
    off_t sigilSize = lseek(fd_sigil, 0, SEEK_END);
    if (sigilSize < 0 || lseek(fd_sigil, 0, SEEK_SET) < 0) {
        close(fd_sigil);
        close(fd_nextHop);
        fillBasicError(response, "Cannot determine sigil size");
        return -1;
    }
    uint8_t *sigBuf = NULL;
    if (sigilSize > 0) {
        sigBuf = malloc((unsigned long)sigilSize);
        if (!sigBuf) {
            close(fd_sigil);
            close(fd_nextHop);
            fillBasicError(response, "Cannot allocate sigil buffer");
            return -1;
        }
        long off = 0;
        long r;
        while (off < sigilSize && (r = read(fd_sigil, sigBuf + off, (unsigned long)(sigilSize - off))) > 0) {
            off += r;
        }
        if (off != sigilSize) {
            free(sigBuf);
            close(fd_sigil);
            close(fd_nextHop);
            fillBasicError(response, "Cannot read sigil file");
            return -1;
        }
    }
    close(fd_sigil);

    char *md5 = md5_buffer(sigBuf, (unsigned long)sigilSize);
    if (!md5) {
        free(sigBuf);
        close(fd_nextHop);
        fillBasicError(response, "Cannot compute sigil md5");
        return -1;
    }

    // Nom del fitxer del segell (sense la ruta)
    const char *slash = strrchr(request->path, '/');
    const char *sigilName = slash ? slash + 1 : request->path;

    // HEADER (0x01): RealmName&SigilName&FileSize&MD5SUM
    char frameData[DATA_MAX_SIZE];
    snprintf(frameData, DATA_MAX_SIZE, "%.63s&%.100s&%lld&%s",
             request->source_realm, sigilName, (long long)sigilSize, md5);
    free(md5);

    Frame requestFrame;
    createFrame(&requestFrame, ALLIANCE_REQUEST, origin, request->target_realm, frameData);
    if (sendFrame(fd_nextHop, &requestFrame) < 0) {
        free(sigBuf);
        close(fd_nextHop);
        fillBasicError(response, "Failed to send alliance request");
        return -1;
    }

    // Esperem ACK FITXER (0x31): B està llest per rebre el segell
    Frame ackFrame;
    if (receiveFrame(fd_nextHop, &ackFrame) < 0 ||
        ackFrame.type != ACK_FILE || strncmp(ackFrame.data, "OK", 2) != 0) {
        free(sigBuf);
        close(fd_nextHop);
        fillBasicError(response, "Sigil not acknowledged");
        return -1;
    }

    // Enviem el segell en blocs binaris (0x02) des del buffer en memòria
    Frame dataFrame;
    long sent = 0;
    while (sent < sigilSize) {
        long remaining = sigilSize - sent;
        uint16_t chunkLen = (remaining > DATA_MAX_SIZE) ? DATA_MAX_SIZE : (uint16_t)remaining;
        createBinaryFrame(&dataFrame, SIGIL_SEND, origin, request->target_realm,
                          sigBuf + sent, chunkLen);
        if (sendFrame(fd_nextHop, &dataFrame) < 0) {
            free(sigBuf);
            close(fd_nextHop);
            fillBasicError(response, "Failed to send sigil data");
            return -1;
        }
        sent += chunkLen;
    }
    free(sigBuf);

    // Rebem ACK MD5SUM (0x32) amb el resultat de la verificació
    Frame checkFrame;
    if (receiveFrame(fd_nextHop, &checkFrame) < 0) {
        close(fd_nextHop);
        fillBasicError(response, "No md5 check for sigil");
        return -1;
    }
    close(fd_nextHop);

    int ok = (checkFrame.type == ACK_MD5SUM && strncmp(checkFrame.data, "CHECK_OK", 8) == 0);
    response->status = ok ? IPC_STATUS_OK : IPC_STATUS_REMOTE_ERROR;
    response->result_code = ok ? 0 : -1;
    response->frame_type = checkFrame.type;
    return ok ? 0 : -1;
}

int envoySendAllianceResponse(const IpcRequest *request, IpcResponse *response) {
    int fd_dest = -1;
    if (connectForRequest(request, &fd_dest) < 0) {
        fillBasicError(response, "Cannot connect to requester");
        return -1;
    }

    char origin[IPC_IP_SIZE + 16];
    char responseData[DATA_MAX_SIZE];
    snprintf(origin, sizeof(origin), "%s:%u", request->source_ip, request->source_port);
    snprintf(responseData, DATA_MAX_SIZE, "%s&%s",
             request->aux_value ? "ACCEPT" : "REJECT", request->source_realm);

    Frame responseFrame;
    createFrame(&responseFrame, ALLIANCE_RESPONSE, origin, request->target_realm, responseData);

    if (sendFrame(fd_dest, &responseFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "Failed to send alliance response");
        return -1;
    }

    Frame ackFrame;
    if (receiveFrame(fd_dest, &ackFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No ACK received");
        return -1;
    }

    close(fd_dest);
    response->status = (ackFrame.type == ACK_FILE) ? IPC_STATUS_OK : IPC_STATUS_REMOTE_ERROR;
    response->frame_type = ackFrame.type;
    response->result_code = (ackFrame.type == ACK_FILE) ? 0 : -1;
    strncpy(response->payload, ackFrame.data, IPC_PAYLOAD_SIZE - 1);
    return (ackFrame.type == ACK_FILE) ? 0 : -1;
}

int envoySendProductListRequest(const IpcRequest *request, IpcResponse *response) {
    int fd_dest = -1;
    if (connectForRequest(request, &fd_dest) < 0) {
        fillBasicError(response, "Cannot connect to target realm");
        return -1;
    }

    char origin[IPC_IP_SIZE + 16];
    snprintf(origin, sizeof(origin), "%s:%u", request->source_ip, request->source_port);

    // PETICIÓ (0x11)
    Frame requestFrame;
    createFrame(&requestFrame, PRODUCT_LIST_REQUEST, origin, request->target_realm, request->source_realm);
    if (sendFrame(fd_dest, &requestFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "Failed to send product list request");
        return -1;
    }

    // HEADER (0x12) o trama d'error (0x25/0x69)
    Frame headerFrame;
    if (receiveFrame(fd_dest, &headerFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No response from target realm");
        return -1;
    }
    if (headerFrame.type != PRODUCT_LIST_HEADER) {
        close(fd_dest);
        response->frame_type = headerFrame.type;
        response->status = IPC_STATUS_REMOTE_ERROR;
        response->result_code = -1;
        strncpy(response->payload, headerFrame.data, IPC_PAYLOAD_SIZE - 1);
        return -1;
    }

    // Parsejar header: FileName&FileSize&MD5SUM
    char fileName[128] = "";
    long fileSize = 0;
    char expectedMd5[64] = "";
    if (sscanf(headerFrame.data, "%127[^&]&%ld&%63s", fileName, &fileSize, expectedMd5) < 3) {
        close(fd_dest);
        fillBasicError(response, "Invalid product list header");
        return -1;
    }

    // ACK FITXER (0x31) -> estem llestos per rebre
    Frame ackFrame;
    char ackData[DATA_MAX_SIZE];
    snprintf(ackData, DATA_MAX_SIZE, "OK&%s", request->source_realm);
    createFrame(&ackFrame, ACK_FILE, "", "", ackData);
    if (sendFrame(fd_dest, &ackFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "Failed to ack product list header");
        return -1;
    }

    // Rebre les DADES (0x13) en un buffer en MEMÒRIA (sense temp file)
    uint8_t *recvBuf = NULL;
    if (fileSize > 0) {
        recvBuf = malloc((unsigned long)fileSize);
        if (!recvBuf) {
            close(fd_dest);
            fillBasicError(response, "Cannot allocate product buffer");
            return -1;
        }
    }

    long received = 0;
    while (received < fileSize) {
        Frame dataFrame;
        if (receiveFrame(fd_dest, &dataFrame) < 0 || dataFrame.type != PRODUCT_LIST_DATA) {
            free(recvBuf);
            close(fd_dest);
            fillBasicError(response, "Failed receiving product list data");
            return -1;
        }
        uint16_t len = dataFrame.data_length;
        if (len > 0 && received + (long)len <= fileSize) {
            memcpy(recvBuf + received, dataFrame.data, len);
        }
        received += len;
    }

    // Verificar md5 (en memòria) i enviar ACK MD5SUM (0x32)
    char *actualMd5 = md5_buffer(recvBuf, (unsigned long)fileSize);
    int md5ok = (actualMd5 && strcmp(actualMd5, expectedMd5) == 0);
    free(actualMd5);

    Frame checkFrame;
    char checkData[DATA_MAX_SIZE];
    snprintf(checkData, DATA_MAX_SIZE, "%s&%s",
             md5ok ? "CHECK_OK" : "CHECK_KO", request->source_realm);
    createFrame(&checkFrame, ACK_MD5SUM, "", "", checkData);
    sendFrame(fd_dest, &checkFrame);
    close(fd_dest);

    if (!md5ok) {
        free(recvBuf);
        fillBasicError(response, "Product list md5 mismatch");
        return -1;
    }

    listRemoteInventoryBuf(request->target_realm, recvBuf, (unsigned long)fileSize);

    response->payload[0] = '\0';
    unsigned long off = 0;
    long nRecords = fileSize / (long)sizeof(AuxiliarProduct);
    for (long i = 0; i < nRecords; i++) {
        AuxiliarProduct aux;
        memcpy(&aux, recvBuf + (unsigned long)i * sizeof(AuxiliarProduct), sizeof(AuxiliarProduct));
        aux.name[sizeof(aux.name) - 1] = '\0';
        char entry[160];
        int el = snprintf(entry, sizeof(entry), "%s%s,%.2f",
                          (off > 0) ? "|" : "", aux.name, aux.weight);
        if (el < 0 || off + (unsigned long)el >= IPC_PAYLOAD_SIZE) {
            break;  // no cap més al payload
        }
        memcpy(response->payload + off, entry, (unsigned long)el);
        off += (unsigned long)el;
        response->payload[off] = '\0';
    }
    free(recvBuf);

    response->status = IPC_STATUS_OK;
    response->frame_type = PRODUCT_LIST_HEADER;
    response->result_code = 0;
    strncpy(response->realm, request->target_realm, IPC_REALM_SIZE - 1);
    return 0;
}


int sendAllianceRequest(Maester *maester, const char *realmName, const char *sigilPath) {
    if (!maester || !realmName || !sigilPath) {
        return -1;
    }
    char *msg;
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

    char *routeIp = NULL;
    int routePort = 0;
    if (!getRouteInfo(maester, realmName, &routeIp, &routePort) || 
        (routeIp && strcasecmp(routeIp, "*.*.*.*") == 0)) {
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

    int envoyIndex = reserveEnvoy(maester);
    if (envoyIndex < 0) {
        asprintf(&msg, RED "ERROR | No envoys available\n" RESET);
        customWrite(1, msg);
        free(msg);
        free(routeIp);
        return -1;
    }

    IpcRequest request;
    IpcResponse response;
    memset(&request, 0, sizeof(IpcRequest));
    memset(&response, 0, sizeof(IpcResponse));
    request.type = IPC_PLEDGE_REQUEST;
    strncpy(request.source_realm, maester->name, IPC_REALM_SIZE - 1);
    strncpy(request.source_ip, maester->ip, IPC_IP_SIZE - 1);
    request.source_port = (uint32_t)maester->port;
    strncpy(request.target_realm, realmName, IPC_REALM_SIZE - 1);
    strncpy(request.target_ip, routeIp, IPC_IP_SIZE - 1);
    request.target_port = (uint32_t)routePort;
    strncpy(request.path, sigilPath, IPC_PATH_SIZE - 1);
    asprintf(&msg, "PLEDGE to %s", realmName);
    setEnvoyMission(maester, envoyIndex, msg);
    free(msg);

    free(routeIp);

    if (dispatchEnvoyRequest(maester, envoyIndex, &request, &response) < 0 || response.status != IPC_STATUS_OK) {
        if (response.payload[0] != '\0') {
            asprintf(&msg, RED "ERROR | Pledge to [%s] failed: %s\n" RESET, realmName, response.payload);
        } else {
            asprintf(&msg, RED "ERROR | Failed to send alliance request to [%s]\n" RESET, realmName);
        }
        customWrite(1, msg);
        free(msg);
        releaseEnvoy(maester, envoyIndex);
        return -1;
    }
    addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_PENDING);
    asprintf(&msg, GREEN "Pledge sent to %s. Envoy %d is on its way.\n" RESET,
             realmName, envoyIndex + 1);
    customWrite(1, msg);
    free(msg);
    return 0;
}

int envoySendTradeFile(const IpcRequest *request, IpcResponse *response) {
    if (!request || !response) {
        return -1;
    }

    int fd_dest = -1;
    if (connectForRequest(request, &fd_dest) < 0) {
        fillBasicError(response, "Cannot connect to target realm");
        return -1;
    }

    int fd_trade = open(request->path, O_RDONLY);
    if (fd_trade < 0) {
        close(fd_dest);
        fillBasicError(response, "Cannot open trade file");
        return -1;
    }

    // Mida del fitxer sense stat (prohibit): lseek al final i tornem a l'inici.
    off_t fileSize = lseek(fd_trade, 0, SEEK_END);
    if (fileSize < 0 || lseek(fd_trade, 0, SEEK_SET) < 0) {
        close(fd_trade);
        close(fd_dest);
        fillBasicError(response, "Cannot determine trade file size");
        return -1;
    }

    // Llegim la comanda SENCERA a memòria (md5 sense temp file)
    uint8_t *tradeBuf = NULL;
    if (fileSize > 0) {
        tradeBuf = malloc((unsigned long)fileSize);
        if (!tradeBuf) {
            close(fd_trade);
            close(fd_dest);
            fillBasicError(response, "Cannot allocate trade buffer");
            return -1;
        }
        long roff = 0;
        long r;
        while (roff < fileSize && (r = read(fd_trade, tradeBuf + roff, (unsigned long)(fileSize - roff))) > 0) {
            roff += r;
        }
        if (roff != fileSize) {
            free(tradeBuf);
            close(fd_trade);
            close(fd_dest);
            fillBasicError(response, "Cannot read trade file");
            return -1;
        }
    }
    close(fd_trade);

    char origin[IPC_IP_SIZE + 16];
    snprintf(origin, sizeof(origin), "%s:%u", request->source_ip, request->source_port);

    char *md5 = md5_buffer(tradeBuf, (unsigned long)fileSize);
    if (!md5) {
        free(tradeBuf);
        close(fd_dest);
        fillBasicError(response, "Cannot compute order md5");
        return -1;
    }

    // HEADER (0x14): FileName&FileSize&MD5SUM
    char headerData[DATA_MAX_SIZE];
    snprintf(headerData, DATA_MAX_SIZE, "order.txt&%lld&%s", (long long)fileSize, md5);
    free(md5);

    Frame headerFrame;
    createFrame(&headerFrame, ORDER_REQUEST_HEADER, origin, request->target_realm, headerData);
    if (sendFrame(fd_dest, &headerFrame) < 0) {
        free(tradeBuf);
        close(fd_dest);
        fillBasicError(response, "Failed to send trade header");
        return -1;
    }

    // Esperem ACK FITXER (0x31)
    Frame ackFrame;
    if (receiveFrame(fd_dest, &ackFrame) < 0 ||
        ackFrame.type != ACK_FILE || strncmp(ackFrame.data, "OK", 2) != 0) {
        free(tradeBuf);
        close(fd_dest);
        fillBasicError(response, "Order not acknowledged");
        return -1;
    }

    // Enviem la comanda en blocs (0x15) des del buffer en memòria
    Frame dataFrame;
    long sent = 0;
    while (sent < fileSize) {
        long remaining = fileSize - sent;
        uint16_t chunkLen = (remaining > DATA_MAX_SIZE) ? DATA_MAX_SIZE : (uint16_t)remaining;
        createBinaryFrame(&dataFrame, ORDER_REQUEST_DATA, origin, request->target_realm,
                          tradeBuf + sent, chunkLen);
        if (sendFrame(fd_dest, &dataFrame) < 0) {
            free(tradeBuf);
            close(fd_dest);
            fillBasicError(response, "Failed to send trade chunk");
            return -1;
        }
        sent += chunkLen;
    }
    free(tradeBuf);

    // Rebem ACK MD5SUM (0x32)
    Frame checkFrame;
    if (receiveFrame(fd_dest, &checkFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No md5 check for order");
        return -1;
    }

    // Rebem RESPOSTA A COMANDA (0x16): OK o REJECT&<reason>
    Frame orderResp;
    if (receiveFrame(fd_dest, &orderResp) < 0) {
        close(fd_dest);
        fillBasicError(response, "No order response");
        return -1;
    }
    close(fd_dest);

    int ok = (orderResp.type == ORDER_RESPONSE && strncmp(orderResp.data, "OK", 2) == 0);
    response->status = ok ? IPC_STATUS_OK : IPC_STATUS_REMOTE_ERROR;
    response->frame_type = orderResp.type;
    response->result_code = ok ? 0 : -1;
    strncpy(response->payload, orderResp.data, IPC_PAYLOAD_SIZE - 1);
    return response->result_code;
}

int sendAllianceResponse(Maester *maester, const char *realmName, int accept) {
    if (!maester || !realmName) {
        return -1;
    }
    
    char *msg;
    
    // Buscar l'aliança PENDING amb aquest regne (thread-safe)
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
    
    // Verificar que tenim IP (vol dir que hem REBUT la petició, no enviat)
    if (!savedIp || savedPort <= 0) {
        asprintf(&msg, RED "ERROR | You sent this request, wait for their response\n" RESET);
        customWrite(1, msg);
        free(msg);
        if (savedIp) free(savedIp);
        return -1;
    }
    int envoyIndex = reserveEnvoy(maester);
    if (envoyIndex < 0) {
        asprintf(&msg, RED "ERROR | No envoys available\n" RESET);
        customWrite(1, msg);
        free(msg);
        free(savedIp);
        return -1;
    }

    IpcRequest request;
    IpcResponse response;
    memset(&request, 0, sizeof(IpcRequest));
    request.type = IPC_PLEDGE_RESPOND;
    request.aux_value = (uint32_t)accept;
    strncpy(request.source_realm, maester->name, IPC_REALM_SIZE - 1);
    strncpy(request.source_ip, maester->ip, IPC_IP_SIZE - 1);
    request.source_port = (uint32_t)maester->port;
    strncpy(request.target_realm, realmName, IPC_REALM_SIZE - 1);
    strncpy(request.target_ip, savedIp, IPC_IP_SIZE - 1);
    request.target_port = (uint32_t)savedPort;
    asprintf(&msg, "PLEDGE RESPOND to %s", realmName);
    setEnvoyMission(maester, envoyIndex, msg);
    free(msg);

    if (dispatchEnvoyRequest(maester, envoyIndex, &request, &response) == 0 &&
        response.status == IPC_STATUS_OK) {
        if (accept) {
            addOrUpdateAlliance(maester, realmName, savedIp, savedPort, ALLIANCE_ACTIVE);
            asprintf(&msg, GREEN "Alliance with %s established.\n" RESET, realmName);
        } else {
            addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_NONE);
            asprintf(&msg, YELLOW "Alliance with %s rejected.\n" RESET, realmName);
        }
        customWrite(1, msg);
        free(msg);
    } else {
        addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_FAILED);
        asprintf(&msg, RED "Alliance with %s failed.\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
    }

    releaseEnvoy(maester, envoyIndex);
    free(savedIp);
    return 0;
}

int sendProductListRequest(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return -1;
    }
    
    char *msg;
    
    char *allyIp = NULL;
    int allyPort = 0;
    int allyStatus = ALLIANCE_NONE;
    
    if (!getAllianceInfo(maester, realmName, &allyIp, &allyPort, &allyStatus, NULL)) {
        asprintf(&msg, RED "ERROR | No alliance info for [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }
    
    //Determinar cómo conectar: directo al aliado o via ruta
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
    int envoyIndex = reserveEnvoy(maester);
    if (envoyIndex < 0) {
        asprintf(&msg, RED "ERROR | No envoys available\n" RESET);
        customWrite(1, msg);
        free(msg);
        free(targetIp);
        return -1;
    }

    IpcRequest request;
    IpcResponse response;
    memset(&request, 0, sizeof(IpcRequest));
    request.type = IPC_LIST_PRODUCTS_REMOTE;
    strncpy(request.source_realm, maester->name, IPC_REALM_SIZE - 1);
    strncpy(request.source_ip, maester->ip, IPC_IP_SIZE - 1);
    request.source_port = (uint32_t)maester->port;
    strncpy(request.target_realm, realmName, IPC_REALM_SIZE - 1);
    strncpy(request.target_ip, targetIp, IPC_IP_SIZE - 1);
    request.target_port = (uint32_t)targetPort;
    asprintf(&msg, "LIST PRODUCTS to %s", realmName);
    setEnvoyMission(maester, envoyIndex, msg);
    free(msg);

    free(targetIp);

    if (dispatchEnvoyRequest(maester, envoyIndex, &request, &response) == 0 &&
        response.status == IPC_STATUS_OK) {
        updateRemoteCatalog(maester, realmName, response.payload);
        releaseEnvoy(maester, envoyIndex);
        return 0;
    } else if (response.frame_type == ERR_UNAUTHORIZED) {
        asprintf(&msg, RED "Error [UNAUTHORIZED]\n" RESET);
        customWrite(1, msg);
        free(msg);
        releaseEnvoy(maester, envoyIndex);
        return -1;
    } else if (response.frame_type == NACK_ERROR) {
        asprintf(&msg, RED "Error [NACK]\n" RESET);
        customWrite(1, msg);
        free(msg);
        releaseEnvoy(maester, envoyIndex);
        return -1;
    } else {
        asprintf(&msg, YELLOW "Unexpected response type 0x%02X\n" RESET, response.frame_type);
        customWrite(1, msg);
        free(msg);
        releaseEnvoy(maester, envoyIndex);
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

int sendPing(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return -1;
    }

    char *msg;
    char *targetIp = NULL;
    int targetPort = 0;

    // Si ja som aliats, tenim la seva IP:Port directa -> anem directes (sense hops).
    char *allyIp = NULL;
    int allyPort = 0;
    int allyStatus = ALLIANCE_NONE;
    if (getAllianceInfo(maester, realmName, &allyIp, &allyPort, &allyStatus, NULL) &&
        allyStatus == ALLIANCE_ACTIVE && allyIp && allyPort > 0) {
        targetIp = allyIp;        // prenem la propietat del punter
        targetPort = allyPort;
    } else {
        if (allyIp) free(allyIp);
        // No aliats (o sense IP directa): resolem per la taula de rutes (hops).
        if (!getRouteInfo(maester, realmName, &targetIp, &targetPort) ||
            (targetIp && strcasecmp(targetIp, "*.*.*.*") == 0)) {
            if (targetIp) free(targetIp);
            targetIp = NULL;
            if (!getRouteInfo(maester, NULL, &targetIp, &targetPort)) {
                asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
                customWrite(1, msg);
                free(msg);
                return -1;
            }
        }
    }

    if (!targetIp) {
        asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        return -1;
    }

    int envoyIndex = reserveEnvoy(maester);
    if (envoyIndex < 0) {
        asprintf(&msg, RED "ERROR | No envoys available\n" RESET);
        customWrite(1, msg);
        free(msg);
        free(targetIp);
        return -1;
    }

    IpcRequest request;
    IpcResponse response;
    memset(&request, 0, sizeof(IpcRequest));
    request.type = IPC_SEND_PING;
    strncpy(request.source_realm, maester->name, IPC_REALM_SIZE - 1);
    strncpy(request.source_ip, maester->ip, IPC_IP_SIZE - 1);
    request.source_port = (uint32_t)maester->port;
    strncpy(request.target_realm, realmName, IPC_REALM_SIZE - 1);
    strncpy(request.target_ip, targetIp, IPC_IP_SIZE - 1);
    request.target_port = (uint32_t)targetPort;

    asprintf(&msg, "PING %s", realmName);
    setEnvoyMission(maester, envoyIndex, msg);
    free(msg);
    free(targetIp);

    if (dispatchEnvoyRequest(maester, envoyIndex, &request, &response) < 0 || response.status != IPC_STATUS_OK) {
        asprintf(&msg, RED "PING [%s]: No response\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        releaseEnvoy(maester, envoyIndex);
        return -1;
    }

    customWrite(1, response.payload);
    customWrite(1, "\n");
    releaseEnvoy(maester, envoyIndex);
    return 0;
}

int envoySendPing(const IpcRequest *request, IpcResponse *response) {
    if (!request || !response) {
        return -1;
    }

    int fd_dest = -1;
    if (connectForRequest(request, &fd_dest) < 0) {
        fillBasicError(response, "Cannot connect to target realm");
        return -1;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char origin[IPC_IP_SIZE + 16];
    snprintf(origin, sizeof(origin), "%s:%u", request->source_ip, request->source_port);

    Frame pingFrame;
    createFrame(&pingFrame, PING_PONG, origin, request->target_realm, "PING");

    if (sendFrame(fd_dest, &pingFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "Failed to send PING");
        return -1;
    }

    Frame pongFrame;
    if (receiveFrame(fd_dest, &pongFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No PONG response");
        return -1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    close(fd_dest);

    long latency_ms = (end.tv_sec - start.tv_sec) * 1000 +
                      (end.tv_nsec - start.tv_nsec) / 1000000;

    if (pongFrame.type != PING_PONG) {
        fillBasicError(response, "Invalid response type");
        return -1;
    }

    response->status = IPC_STATUS_OK;
    response->frame_type = PING_PONG;
    response->result_code = (int)latency_ms;
    snprintf(response->payload, IPC_PAYLOAD_SIZE,
             "PING [%s]: %d ms", request->target_realm, (int)latency_ms);
    return 0;
}
