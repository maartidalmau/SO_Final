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

    //Implementar funcio per llegir els fitxers de configuracio dels maesters
    readConfigFile(argv[1], maesterData); //TODO COMPROBAR RETURN

    if (maesterData == NULL) {
        asprintf(&msg, "%sERROR | Cannot open file %s%s\n", RED, argv[1], RESET);
        customWrite(1, msg);
        free(msg);

        return 1;
    }

    //Implementar funcior per llegir el fitxer data
    readProducts(argv[2], maesterData); //TODO COMPROBAR RETURN

    if (maesterData->inventory == NULL) {
        asprintf(&msg, "%sERROR | Cannot open file %s%s\n", RED, argv[2], RESET);
        customWrite(1, msg);
        free(msg);
        //freeMaesterData(&maesterData);

        return 1;
    }

    asprintf(&msg, "%sMaester of %s initialized. The board is set.%s\n", GREEN, maesterData->name, RESET);
    customWrite(1, msg);
    free(msg);

    consoleLogic(maesterData);

    asprintf(&msg, "%sThe Maester of %s signs off. The ravens rest.%s\n", MAGENTA, maesterData->name, RESET);
    customWrite(1, msg);
    free(msg);    

    return 0;
}