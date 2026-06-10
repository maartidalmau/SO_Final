#include "envoy.h"
#include "client.h"
#include "utils.h"

#include <signal.h>

static IpcResponse executeEnvoyRequest(const IpcRequest *request) {
    IpcResponse response;
    memset(&response, 0, sizeof(IpcResponse));
    response.request_id = request->request_id;
    strncpy(response.realm, request->target_realm, IPC_REALM_SIZE - 1);
    response.status = IPC_STATUS_ERROR;
    response.result_code = -1;

    switch (request->type) {
        case IPC_PLEDGE_REQUEST:
            envoySendAllianceRequest(request, &response);
            break;
        case IPC_PLEDGE_RESPOND:
            envoySendAllianceResponse(request, &response);
            break;
        case IPC_LIST_PRODUCTS_REMOTE:
            envoySendProductListRequest(request, &response);
            break;
        case IPC_SEND_SIGIL:
            envoySendSigilFile(request, &response);
            break;
        case IPC_SEND_TRADE_FILE:
            envoySendTradeFile(request, &response);
            break;
        case IPC_SEND_PING:
            envoySendPing(request, &response);
            break;
        case IPC_SHUTDOWN:
            response.status = IPC_STATUS_OK;
            response.result_code = 0;
            break;
        default:
            strncpy(response.payload, "Unsupported IPC request", IPC_PAYLOAD_SIZE - 1);
            break;
    }

    return response;
}

void envoyProcess(Envoy envoy) {
    signal(SIGINT, SIG_IGN);

    while (1) {
        IpcRequest request;
        if (receiveIpcRequest(envoy.p2c, &request) < 0) {
            break;
        }

        IpcResponse response = executeEnvoyRequest(&request);
        if (sendIpcResponse(envoy.c2p, &response) < 0) {
            break;
        }

        if (request.type == IPC_SHUTDOWN) {
            break;
        }
    }

    close(envoy.p2c);
    close(envoy.c2p);
}

void createEnvoys(Maester *maester){
    maester->envoyPInfo.p2c = malloc(maester->envoys * sizeof(int*));
    maester->envoyPInfo.c2p = malloc(maester->envoys * sizeof(int*));
    maester->envoyPInfo.envoyPIDs = malloc(maester->envoys * sizeof(pid_t));
    maester->envoysAvailable = malloc(maester->envoys * sizeof(int));
    maester->envoyMissions = malloc(maester->envoys * sizeof(char *));

    for (int i = 0; i < maester->envoys; i++) {
        maester->envoyPInfo.p2c[i] = malloc(2 * sizeof(int));
        maester->envoyPInfo.c2p[i] = malloc(2 * sizeof(int));
        pipe(maester->envoyPInfo.p2c[i]);
        pipe(maester->envoyPInfo.c2p[i]);
        maester->envoyPInfo.envoyPIDs[i] = -1;
        maester->envoysAvailable[i] = 1;
        maester->envoyMissions[i] = NULL;
    }

    for (int i = 0; i < maester->envoys; i++) {
        maester->envoyPInfo.envoyPIDs[i] = fork();

        if (maester->envoyPInfo.envoyPIDs[i] == 0) {
            for (int j = 0; j < maester->envoys; j++) {
                if (j != i) {
                    close(maester->envoyPInfo.p2c[j][0]);
                    close(maester->envoyPInfo.p2c[j][1]);
                    close(maester->envoyPInfo.c2p[j][0]);
                    close(maester->envoyPInfo.c2p[j][1]);
                }
            }
            close(maester->envoyPInfo.p2c[i][1]);
            close(maester->envoyPInfo.c2p[i][0]);

            Envoy envoy;
            envoy.p2c = maester->envoyPInfo.p2c[i][0];
            envoy.c2p = maester->envoyPInfo.c2p[i][1];

            envoyProcess(envoy);

            // En sortir, l'envoy allibera tota la memòria heretada del Maester
            // (config, productes, rutes, arrays de pipes...) per no deixar fuites.
            destroyEnvoys(maester);
            destroyMaester(maester);
            exit(0);
        } else if (maester->envoyPInfo.envoyPIDs[i] > 0) {
            close(maester->envoyPInfo.p2c[i][0]);
            close(maester->envoyPInfo.c2p[i][1]);
        } else {
            customWrite(1, RED "ERROR | Fork failed\n" RESET);
            for (int j = 0; j < i; j++) {
                close(maester->envoyPInfo.p2c[j][0]);
                close(maester->envoyPInfo.p2c[j][1]);
                close(maester->envoyPInfo.c2p[j][0]);
                close(maester->envoyPInfo.c2p[j][1]);
            }
            for (int j = 0; j < i; j++) {
                if (maester->envoyPInfo.envoyPIDs[j] > 0) {
                    kill(maester->envoyPInfo.envoyPIDs[j], SIGKILL);
                    waitpid(maester->envoyPInfo.envoyPIDs[j], NULL, 0);
                }
            }
            destroyMaester(maester);
            return;
        }
    }
}

int reserveEnvoy(Maester *maester) {
    int chosenEnvoy = -1;

    pthread_mutex_lock(&maester->workersInfo->workers_mutex);
    for (int i = 0; i < maester->envoys; i++) {
        if (maester->envoysAvailable[i] == 1) {
            maester->envoysAvailable[i] = 0;
            chosenEnvoy = i;
            break;
        }
    }
    pthread_mutex_unlock(&maester->workersInfo->workers_mutex);

    return chosenEnvoy;
}

void releaseEnvoy(Maester *maester, int envoyIndex) {
    if (!maester || envoyIndex < 0 || envoyIndex >= maester->envoys) {
        return;
    }

    pthread_mutex_lock(&maester->workersInfo->workers_mutex);
    maester->envoysAvailable[envoyIndex] = 1;
    safeFree((void **)&maester->envoyMissions[envoyIndex]);
    pthread_mutex_unlock(&maester->workersInfo->workers_mutex);
}

void setEnvoyMission(Maester *maester, int envoyIndex, const char *mission) {
    if (!maester || envoyIndex < 0 || envoyIndex >= maester->envoys) {
        return;
    }

    pthread_mutex_lock(&maester->workersInfo->workers_mutex);
    safeFree((void **)&maester->envoyMissions[envoyIndex]);
    maester->envoyMissions[envoyIndex] = mission ? strdup(mission) : NULL;
    pthread_mutex_unlock(&maester->workersInfo->workers_mutex);
}

void releaseEnvoyMissionForRealm(Maester *maester, const char *realmName, const char *missionPrefix) {
    if (!maester || !realmName || !missionPrefix) {
        return;
    }

    pthread_mutex_lock(&maester->workersInfo->workers_mutex);
    for (int i = 0; i < maester->envoys; i++) {
        if (!maester->envoyMissions[i]) {
            continue;
        }
        if (strstr(maester->envoyMissions[i], missionPrefix) == maester->envoyMissions[i] &&
            strstr(maester->envoyMissions[i], realmName) != NULL) {
            maester->envoysAvailable[i] = 1;
            safeFree((void **)&maester->envoyMissions[i]);
            break;
        }
    }
    pthread_mutex_unlock(&maester->workersInfo->workers_mutex);
}

int dispatchEnvoyRequest(Maester *maester, int envoyIndex, const IpcRequest *request, IpcResponse *response) {
    if (!maester || !request || !response) {
        return -1;
    }

    if (sendIpcRequest(maester->envoyPInfo.p2c[envoyIndex][1], request) < 0) {
        return -1;
    }

    if (receiveIpcResponse(maester->envoyPInfo.c2p[envoyIndex][0], response) < 0) {
        return -1;
    }

    return 0;
}
