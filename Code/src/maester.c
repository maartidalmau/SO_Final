#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "dataStructures.h"
#include "utils.h"
#include "console.h"
#include "server.h"
#include "client.h"
#include "envoy.h"

volatile sig_atomic_t *running = NULL;
volatile sig_atomic_t *envoyRunning = NULL;

void rsiCtrlC() {
    if (running) {
        *running = 0;
    }
}
void rsiShutdownEnvoy() {
    if (envoyRunning) {
        *envoyRunning = 0;
    }
}

int reserveMemoryEnvoysAvailable(Maester *maester) {
    int memid = shmget(IPC_PRIVATE, sizeof(int) * maester->envoys, IPC_CREAT | IPC_EXCL| 0600);
    if (memid < 0) {
        return 1;
    }

    maester->envoysAvailable = shmat(memid, NULL, 0);
    if (maester->envoysAvailable == (void*)-1) {
        shmctl(memid, IPC_RMID, NULL);
        return 1;
    }
    return 0;
}

void destroyEnvoysAvailable(Maester *maester) {
    if (maester->envoysAvailable) {
        shmdt((void*)maester->envoysAvailable);
        maester->envoysAvailable = NULL;
    }
}

int loadMaesterData(Maester **maester, char *configFile, char *stockFile) {
    char *msg;
    *maester = malloc(sizeof(Maester));

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

int envoysUtilities(Maester *maester, int memid_envoy){
    memid_envoy = shmget(IPC_PRIVATE, sizeof(volatile sig_atomic_t), IPC_CREAT|IPC_EXCL|0600);
    if (memid_envoy > 0){
        envoyRunning = (volatile sig_atomic_t *)shmat(memid_envoy, NULL, 0);
        *envoyRunning = 1;
    }else{
        customWrite(1, RED "ERROR | Cannot create shared memory\n" RESET);
        return 1;
    }

    struct sigaction sa2;    
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = rsiShutdownEnvoy;
    sa2.sa_flags = 0;
    sigemptyset(&sa2.sa_mask);
    sigaction(SIGUSR1, &sa2, NULL);

    createEnvoys(maester, envoyRunning);

    struct sigaction sa;    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = rsiCtrlC;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
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

    int memid_envoy = -1;
    if (envoysUtilities(maester, memid_envoy)){
        destroyMaester(maester);
        return 1;
    }

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
    
    //Apartir de aqui todo es desconexion, liberar memoria y terminar procesos
    notifyDisconnect(maester);
    endAndCleanEnvoys(maester);

    if (envoyRunning != NULL) {
        shmdt((void*)envoyRunning);
    }
    if (memid_envoy > 0) {
        shmctl(memid_envoy, IPC_RMID, NULL);
    }

    if (maester->serverSocket >= 0) {
        shutdown(maester->serverSocket, SHUT_RDWR);
    }
    cerrarWorkers(maester);

    pthread_join(serverThreadID, NULL);

    asprintf(&msg, GREEN "The Maester of %s signs off. The ravens rest.\n" RESET, maester->name);
    customWrite(1, msg);
    free(msg);
    
    destroyMaester(maester);
    return 0;
}