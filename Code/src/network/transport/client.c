#include "client.h"
#include "envoy.h"

#include <sys/stat.h>

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

static int extractProductPayload(const IpcResponse *response, char *realmName, size_t realmNameSize, char *products, size_t productsSize) {
    char *copy;
    char *cursor;
    char *status;
    char *realm;
    char *productList;

    if (!response || !realmName || !products) {
        return -1;
    }

    copy = strdup(response->payload);
    if (!copy) {
        return -1;
    }

    cursor = copy;
    status = strsep(&cursor, "&");
    realm = strsep(&cursor, "&");
    productList = cursor;

    if (!status || !realm || !productList || strcmp(status, "OK") != 0) {
        free(copy);
        return -1;
    }

    strncpy(realmName, realm, realmNameSize - 1);
    realmName[realmNameSize - 1] = '\0';
    strncpy(products, productList, productsSize - 1);
    products[productsSize - 1] = '\0';
    free(copy);
    return 0;
}

int envoySendAllianceRequest(const IpcRequest *request, IpcResponse *response) {
    int fd_nextHop = -1;
    if (connectForRequest(request, &fd_nextHop) < 0) {
        fillBasicError(response, "Cannot connect to route");
        return -1;
    }

    char origin[IP_SIZE];
    char frameData[DATA_MAX_SIZE];
    snprintf(origin, IP_SIZE, "%s:%u", request->source_ip, request->source_port);
    snprintf(frameData, DATA_MAX_SIZE, "%.63s&%.100s&0&00000000000000000000000000000000",
             request->source_realm, request->path);

    Frame requestFrame;
    createFrame(&requestFrame, ALLIANCE_REQUEST, origin, request->target_realm, frameData);

    if (sendFrame(fd_nextHop, &requestFrame) < 0) {
        close(fd_nextHop);
        fillBasicError(response, "Failed to send alliance request");
        return -1;
    }

    close(fd_nextHop);
    response->status = IPC_STATUS_OK;
    response->result_code = 0;
    return 0;
}

int envoySendAllianceResponse(const IpcRequest *request, IpcResponse *response) {
    int fd_dest = -1;
    if (connectForRequest(request, &fd_dest) < 0) {
        fillBasicError(response, "Cannot connect to requester");
        return -1;
    }

    char origin[IP_SIZE];
    char responseData[DATA_MAX_SIZE];
    snprintf(origin, IP_SIZE, "%s:%u", request->source_ip, request->source_port);
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

    char origin[IP_SIZE];
    snprintf(origin, IP_SIZE, "%s:%u", request->source_ip, request->source_port);

    Frame requestFrame;
    createFrame(&requestFrame, PRODUCT_LIST_REQUEST, origin, request->target_realm, request->source_realm);

    if (sendFrame(fd_dest, &requestFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "Failed to send product list request");
        return -1;
    }

    Frame responseFrame;
    if (receiveFrame(fd_dest, &responseFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No response from target realm");
        return -1;
    }

    close(fd_dest);
    response->frame_type = responseFrame.type;
    response->result_code = (responseFrame.type == ACK_FILE) ? 0 : -1;
    response->status = (responseFrame.type == ACK_FILE) ? IPC_STATUS_OK : IPC_STATUS_REMOTE_ERROR;
    strncpy(response->payload, responseFrame.data, IPC_PAYLOAD_SIZE - 1);
    return (responseFrame.type == ACK_FILE) ? 0 : -1;
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
        asprintf(&msg, RED "ERROR | Failed to send alliance request to [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        releaseEnvoy(maester, envoyIndex);
        return -1;
    }
    addOrUpdateAlliance(maester, realmName, NULL, 0, ALLIANCE_PENDING);
    setAllianceSigil(maester, realmName, sigilPath);
    return 0;
}



// ═══════════════════════════════════════════════════════════
// STUBS: Funciones pendientes de implementar
// ═══════════════════════════════════════════════════════════

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

    struct stat st;
    if (fstat(fd_trade, &st) < 0) {
        close(fd_trade);
        close(fd_dest);
        fillBasicError(response, "Cannot stat trade file");
        return -1;
    }

    char origin[IP_SIZE];
    snprintf(origin, IP_SIZE, "%s:%u", request->source_ip, request->source_port);

    char headerData[DATA_MAX_SIZE];
    snprintf(headerData, DATA_MAX_SIZE, "%lld", (long long)st.st_size);

    Frame headerFrame;
    createFrame(&headerFrame, ORDER_REQUEST_HEADER, origin, request->target_realm, headerData);
    if (sendFrame(fd_dest, &headerFrame) < 0) {
        close(fd_trade);
        close(fd_dest);
        fillBasicError(response, "Failed to send trade header");
        return -1;
    }

    uint8_t chunk[DATA_MAX_SIZE];
    ssize_t bytesRead;
    Frame dataFrame;

    while ((bytesRead = read(fd_trade, chunk, DATA_MAX_SIZE)) > 0) {
        createFrame(&dataFrame, ORDER_REQUEST_DATA, origin, request->target_realm, (const char *)chunk);
        dataFrame.data_length = htons((uint16_t)bytesRead);

        if (sendFrame(fd_dest, &dataFrame) < 0) {
            close(fd_trade);
            close(fd_dest);
            fillBasicError(response, "Failed to send trade chunk");
            return -1;
        }
    }

    close(fd_trade);

    Frame ackFrame;
    if (receiveFrame(fd_dest, &ackFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No ACK received for trade");
        return -1;
    }

    close(fd_dest);

    if (ackFrame.type == ORDER_RESPONSE) {
        response->status = (strstr(ackFrame.data, "OK") != NULL) ? IPC_STATUS_OK : IPC_STATUS_REMOTE_ERROR;
    } else {
        response->status = IPC_STATUS_REMOTE_ERROR;
    }

    response->frame_type = ackFrame.type;
    response->result_code = (response->status == IPC_STATUS_OK) ? 0 : -1;
    strncpy(response->payload, ackFrame.data, IPC_PAYLOAD_SIZE - 1);

    return response->result_code;
}

int envoySendSigilFile(const IpcRequest *request, IpcResponse *response) {
    if (!request || !response) {
        return -1;
    }

    int fd_dest = -1;
    if (connectForRequest(request, &fd_dest) < 0) {
        fillBasicError(response, "Cannot connect to target realm");
        return -1;
    }

    int fd_sigil = open(request->path, O_RDONLY);
    if (fd_sigil < 0) {
        close(fd_dest);
        fillBasicError(response, "Cannot open sigil file");
        return -1;
    }

    struct stat st;
    if (fstat(fd_sigil, &st) < 0) {
        close(fd_sigil);
        close(fd_dest);
        fillBasicError(response, "Cannot stat sigil file");
        return -1;
    }

    char origin[IP_SIZE];
    snprintf(origin, IP_SIZE, "%s:%u", request->source_ip, request->source_port);

    uint8_t chunk[DATA_MAX_SIZE];
    ssize_t bytesRead;
    Frame sigilFrame;

    while ((bytesRead = read(fd_sigil, chunk, DATA_MAX_SIZE)) > 0) {
        createFrame(&sigilFrame, SIGIL_SEND, origin, request->target_realm, (const char *)chunk);
        sigilFrame.data_length = htons((uint16_t)bytesRead);

        if (sendFrame(fd_dest, &sigilFrame) < 0) {
            close(fd_sigil);
            close(fd_dest);
            fillBasicError(response, "Failed to send sigil chunk");
            return -1;
        }
    }

    close(fd_sigil);

    Frame ackFrame;
    if (receiveFrame(fd_dest, &ackFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No ACK received for sigil");
        return -1;
    }

    close(fd_dest);
    response->status = (ackFrame.type == ACK_FILE || ackFrame.type == ACK_MD5SUM) ? IPC_STATUS_OK : IPC_STATUS_REMOTE_ERROR;
    response->frame_type = ackFrame.type;
    response->result_code = (ackFrame.type == ACK_FILE || ackFrame.type == ACK_MD5SUM) ? 0 : -1;
    strncpy(response->payload, ackFrame.data, IPC_PAYLOAD_SIZE - 1);
    return response->result_code;
}

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
        char catalogRealm[IPC_REALM_SIZE];
        char serializedProducts[IPC_PAYLOAD_SIZE];
        if (extractProductPayload(&response, catalogRealm, sizeof(catalogRealm), serializedProducts, sizeof(serializedProducts)) == 0) {
            updateRemoteCatalog(maester, catalogRealm, serializedProducts);
        }
        asprintf(&msg, GREEN "Product list request acknowledged by [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        if (findRemoteCatalog(maester, realmName) != NULL) {
            customWrite(1, GREEN "Remote catalog cached for trade session.\n" RESET);
        } else {
            customWrite(1, YELLOW "(Product list processing will be available in Phase 3)\n" RESET);
        }
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
