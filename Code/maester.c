//ports 8095-8099 --> marti
//ports 8395-8399 --> unai
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dataStructures.h"
#include "utils.h"
#include "commands.h"

Maester *maesterData = NULL;

void rsiCtrlC() {
    if (maesterData) {
        maesterData->running = 0;
    }
    raise(SIGINT);
}

int loadMaesterData(char *configFile, char *stockFile) {
    char *msg;
    maesterData = malloc(sizeof(Maester));

    if (readConfigFile(configFile, maesterData)) {
        asprintf(&msg, "%sERROR | Cannot open file %s%s\n", RED, configFile, RESET);
        customWrite(1, msg);
        free(msg);
        return 1;
    }

    if (readProducts(stockFile, maesterData)) {
        asprintf(&msg, "%sERROR | Cannot open file %s%s\n", RED, stockFile, RESET);
        customWrite(1, msg);
        free(msg);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = rsiCtrlC;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);


    char *msg;
    if (argc != 3) {
        asprintf(&msg, "%sERROR | Use %s <Maester file> <Stock file>%s\n", RED, argv[0], RESET);
        customWrite(1, msg);
        free(msg);
        return 1;
    }

    if (loadMaesterData(argv[1], argv[2])) {
        return 1;
    }

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL); //CREC QUE SHA DE BORRAR

    consoleLogic(maesterData);

    return 0;
}