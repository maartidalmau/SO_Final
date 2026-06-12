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

    // Obrim el segell i en calculem mida (sense stat) i md5 (md5sum)
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
    char *md5 = md5_file(request->path);
    if (!md5) {
        close(fd_sigil);
        close(fd_nextHop);
        fillBasicError(response, "Cannot compute sigil md5");
        return -1;
    }

    // Nom del fitxer del segell (sense la ruta)
    const char *slash = strrchr(request->path, '/');
    const char *sigilName = slash ? slash + 1 : request->path;

    // 1. HEADER (0x01): RealmName&SigilName&FileSize&MD5SUM
    char frameData[DATA_MAX_SIZE];
    snprintf(frameData, DATA_MAX_SIZE, "%.63s&%.100s&%lld&%s",
             request->source_realm, sigilName, (long long)sigilSize, md5);
    free(md5);

    Frame requestFrame;
    createFrame(&requestFrame, ALLIANCE_REQUEST, origin, request->target_realm, frameData);
    if (sendFrame(fd_nextHop, &requestFrame) < 0) {
        close(fd_sigil);
        close(fd_nextHop);
        fillBasicError(response, "Failed to send alliance request");
        return -1;
    }

    // 2. Esperem ACK FITXER (0x31): B està llest per rebre el segell
    Frame ackFrame;
    if (receiveFrame(fd_nextHop, &ackFrame) < 0 ||
        ackFrame.type != ACK_FILE || strncmp(ackFrame.data, "OK", 2) != 0) {
        close(fd_sigil);
        close(fd_nextHop);
        fillBasicError(response, "Sigil not acknowledged");
        return -1;
    }

    // 3. Enviem el segell en blocs binaris (0x02)
    uint8_t chunk[DATA_MAX_SIZE];
    ssize_t bytesRead;
    Frame dataFrame;
    while ((bytesRead = read(fd_sigil, chunk, DATA_MAX_SIZE)) > 0) {
        createBinaryFrame(&dataFrame, SIGIL_SEND, origin, request->target_realm,
                          chunk, (uint16_t)bytesRead);
        if (sendFrame(fd_nextHop, &dataFrame) < 0) {
            close(fd_sigil);
            close(fd_nextHop);
            fillBasicError(response, "Failed to send sigil data");
            return -1;
        }
    }
    close(fd_sigil);

    // 4. Rebem ACK MD5SUM (0x32) amb el resultat de la verificació
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

    // 1. PETICIÓ (0x11)
    Frame requestFrame;
    createFrame(&requestFrame, PRODUCT_LIST_REQUEST, origin, request->target_realm, request->source_realm);
    if (sendFrame(fd_dest, &requestFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "Failed to send product list request");
        return -1;
    }

    // 2. HEADER (0x12) o trama d'error (0x25/0x69)
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

    // 3. ACK FITXER (0x31) -> estem llestos per rebre
    Frame ackFrame;
    char ackData[DATA_MAX_SIZE];
    snprintf(ackData, DATA_MAX_SIZE, "OK&%s", request->source_realm);
    createFrame(&ackFrame, ACK_FILE, "", "", ackData);
    if (sendFrame(fd_dest, &ackFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "Failed to ack product list header");
        return -1;
    }

    // 4. Rebre les DADES (0x13) i escriure-les a un fitxer temporal
    char recvPath[256];
    snprintf(recvPath, sizeof(recvPath), "/tmp/products_recv_%s_%d_%d.db",
             request->target_realm, (int)getpid(), fd_dest);
    int fd_file = open(recvPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_file < 0) {
        close(fd_dest);
        fillBasicError(response, "Cannot create temp product file");
        return -1;
    }

    long received = 0;
    while (received < fileSize) {
        Frame dataFrame;
        if (receiveFrame(fd_dest, &dataFrame) < 0 || dataFrame.type != PRODUCT_LIST_DATA) {
            close(fd_file);
            close(fd_dest);
            unlink(recvPath);
            fillBasicError(response, "Failed receiving product list data");
            return -1;
        }
        uint16_t len = dataFrame.data_length;
        if (len > 0 && write(fd_file, dataFrame.data, len) < 0) {
            close(fd_file);
            close(fd_dest);
            unlink(recvPath);
            fillBasicError(response, "Failed writing product list data");
            return -1;
        }
        received += len;
    }
    close(fd_file);

    // 5. Verificar md5 i enviar ACK MD5SUM (0x32)
    char *actualMd5 = md5_file(recvPath);
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
        unlink(recvPath);
        fillBasicError(response, "Product list md5 mismatch");
        return -1;
    }

    // Èxit: retornem la ruta del fitxer rebut perquè el Maester el mostri/parsegi
    response->status = IPC_STATUS_OK;
    response->frame_type = PRODUCT_LIST_HEADER;
    response->result_code = 0;
    strncpy(response->realm, request->target_realm, IPC_REALM_SIZE - 1);
    strncpy(response->payload, recvPath, IPC_PAYLOAD_SIZE - 1);
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

    // El segell ja s'ha lliurat; l'envoy queda ON_MISSION esperant la decisió
    // de l'aliat (que arribarà de forma asíncrona al servidor amb el 0x03).
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

    char origin[IPC_IP_SIZE + 16];
    snprintf(origin, sizeof(origin), "%s:%u", request->source_ip, request->source_port);

    char *md5 = md5_file(request->path);
    if (!md5) {
        close(fd_trade);
        close(fd_dest);
        fillBasicError(response, "Cannot compute order md5");
        return -1;
    }

    // 1. HEADER (0x14): FileName&FileSize&MD5SUM
    char headerData[DATA_MAX_SIZE];
    snprintf(headerData, DATA_MAX_SIZE, "order.txt&%lld&%s", (long long)fileSize, md5);
    free(md5);

    Frame headerFrame;
    createFrame(&headerFrame, ORDER_REQUEST_HEADER, origin, request->target_realm, headerData);
    if (sendFrame(fd_dest, &headerFrame) < 0) {
        close(fd_trade);
        close(fd_dest);
        fillBasicError(response, "Failed to send trade header");
        return -1;
    }

    // 2. Esperem ACK FITXER (0x31)
    Frame ackFrame;
    if (receiveFrame(fd_dest, &ackFrame) < 0 ||
        ackFrame.type != ACK_FILE || strncmp(ackFrame.data, "OK", 2) != 0) {
        close(fd_trade);
        close(fd_dest);
        fillBasicError(response, "Order not acknowledged");
        return -1;
    }

    // 3. Enviem la comanda en blocs (0x15)
    uint8_t chunk[DATA_MAX_SIZE];
    ssize_t bytesRead;
    Frame dataFrame;
    while ((bytesRead = read(fd_trade, chunk, DATA_MAX_SIZE)) > 0) {
        createBinaryFrame(&dataFrame, ORDER_REQUEST_DATA, origin, request->target_realm,
                          chunk, (uint16_t)bytesRead);
        if (sendFrame(fd_dest, &dataFrame) < 0) {
            close(fd_trade);
            close(fd_dest);
            fillBasicError(response, "Failed to send trade chunk");
            return -1;
        }
    }
    close(fd_trade);

    // 4. Rebem ACK MD5SUM (0x32)
    Frame checkFrame;
    if (receiveFrame(fd_dest, &checkFrame) < 0) {
        close(fd_dest);
        fillBasicError(response, "No md5 check for order");
        return -1;
    }

    // 5. Rebem RESPOSTA A COMANDA (0x16): OK o REJECT&<reason>
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
        // response.payload conté la ruta del fitxer temporal amb l'inventari rebut.
        // El mostrem amb el mateix format que els productes propis i cachegem els
        // noms per a poder validar-los a START TRADE.
        listRemoteInventory(realmName, response.payload);
        cacheRemoteCatalogFromFile(maester, realmName, response.payload);
        unlink(response.payload);
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
