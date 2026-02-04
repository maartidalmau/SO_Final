#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dataStructures.h"
#include "utils.h"
#include "console.h"
#include "server.h"
#include "client.h"

volatile sig_atomic_t *running = NULL;

void rsiCtrlC() {
    if (running) {
        *running = 0;
    }
}

int loadMaesterData(Maester **maester, char *configFile, char *stockFile) {
    char *msg;
    *maester = malloc(sizeof(Maester));
    if (!*maester) {
        customWrite(1, RED "ERROR | Cannot allocate memory for Maester\n" RESET);
        return 1;
    }

    if (readConfigFile(configFile, *maester)) {
        asprintf(&msg, "%sERROR | Cannot open file %s%s\n", RED, configFile, RESET);
        customWrite(1, msg);
        free(msg);
        free(*maester);  // Alliberar memòria assignada
        *maester = NULL;
        return 1;
    }

    if (readProducts(stockFile, *maester)) {
        asprintf(&msg, "%sERROR | Cannot open file %s%s\n", RED, stockFile, RESET);
        customWrite(1, msg);
        free(msg);
        destroyMaester(*maester);  // Alliberar tot (config ja carregada)
        *maester = NULL;
        return 1;
    }

    running = &((*maester)->running);

    return 0;
}

int main(int argc, char *argv[]) {
    char *msg;
    if (argc != 3) {
        asprintf(&msg, "%sERROR | Use %s <Maester file> <Stock file>%s\n", RED, argv[0], RESET);
        customWrite(1, msg);
        free(msg);
        return 1;
    }

    Maester *maester = NULL;
    if (loadMaesterData(&maester, argv[1], argv[2])) {
        return 1;
    }

    //We allocate memory for the pipes and the envoy PIDs
    EnvoyPInfo envoyInfo;
    envoyInfo.p2c = malloc(maester->envoys * sizeof(int*));
    envoyInfo.c2p = malloc(maester->envoys * sizeof(int*));
    envoyInfo.envoyPIDs = malloc(maester->envoys * sizeof(pid_t));

    //We initialize the pipes
    for (int i = 0; i < maester->envoys; i++) {
        envoyInfo.p2c[i] = malloc(2 * sizeof(int));
        envoyInfo.c2p[i] = malloc(2 * sizeof(int));
        pipe(envoyInfo.p2c[i]);
        pipe(envoyInfo.c2p[i]);
    }

    for (int i = 0; i < maester->envoys; i++) {
        envoyInfo.envoyPIDs[i] = fork();

        if (envoyInfo.envoyPIDs[i] == 0) {
            //Envoy process
            for (int j = 0; j < maester->envoys; j++) {
                //Close other pipes
                if (j != i) {
                    close(envoyInfo.p2c[j][0]);
                    close(envoyInfo.p2c[j][1]);
                    close(envoyInfo.c2p[j][0]);
                    close(envoyInfo.c2p[j][1]);
                }
                //Close own pipes we don't use
                close(envoyInfo.p2c[i][1]);
                close(envoyInfo.c2p[i][0]);
            }
        } else if (envoyInfo.envoyPIDs[i] > 0) {
            //Maester process
            close(envoyInfo.p2c[i][0]);
            close(envoyInfo.c2p[i][1]);
        } else {
            customWrite(1, RED "ERROR | Fork failed\n" RESET);
            destroyMaester(maester);
            return 1;
        }
    }

    struct sigaction sa;    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = rsiCtrlC;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);



    sigaction(SIGINT, &sa, NULL);

    // Crear thread del servidor
    pthread_t serverThreadID;
    if (pthread_create(&serverThreadID, NULL, serverThread, (void *)maester) != 0) {
        customWrite(1, RED "ERROR | Cannot create server thread\n" RESET);
        destroyMaester(maester);
        return 1;
    }

    if (maester->running) {
        consoleLogic(maester);
    }
    
    notifyDisconnect(maester);
    
    if (maester->serverSocket >= 0) {
        shutdown(maester->serverSocket, SHUT_RDWR);
    }

    // Esperar que el thread del servidor acabi (ell tanca el socket)
    pthread_join(serverThreadID, NULL);

    asprintf(&msg, GREEN "The Maester of %s signs off. The ravens rest.\n" RESET, maester->name);
    customWrite(1, msg);
    free(msg);
    
    destroyMaester(maester);
    maester = NULL;

    return 0;
}