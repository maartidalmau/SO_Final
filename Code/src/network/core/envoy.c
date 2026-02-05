#include "envoy.h"

#include <stdio.h>

volatile sig_atomic_t *envoyRunning = NULL;

void rsiShutdownEnvoy() {
    if (envoyRunning) {
        *envoyRunning = 0;
    }
}

void envoyProcess(Envoy envoy) {
    struct sigaction sa;    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = rsiShutdownEnvoy;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    //Ignore Ctrl+C
    signal(SIGINT, SIG_IGN);

    envoyRunning = &(envoy.running);
    envoy.running = 1;

    while(envoy.running) {
        pause();
    }
    
    //Close pipes and exit
    close(envoy.p2c);
    close(envoy.c2p);
    exit(0);
}

void createEnvoys(Maester *maester){
    //We allocate memory for the pipes and the envoy PIDs
    maester->envoyPInfo.p2c = malloc(maester->envoys * sizeof(int*));
    maester->envoyPInfo.c2p = malloc(maester->envoys * sizeof(int*));
    maester->envoyPInfo.envoyPIDs = malloc(maester->envoys * sizeof(pid_t));

    //We initialize the pipes
    for (int i = 0; i < maester->envoys; i++) {
        maester->envoyPInfo.p2c[i] = malloc(2 * sizeof(int));
        maester->envoyPInfo.c2p[i] = malloc(2 * sizeof(int));
        pipe(maester->envoyPInfo.p2c[i]);
        pipe(maester->envoyPInfo.c2p[i]);
        maester->envoyPInfo.envoyPIDs[i] = -1;
    }

    for (int i = 0; i < maester->envoys; i++) {
        maester->envoyPInfo.envoyPIDs[i] = fork();

        if (maester->envoyPInfo.envoyPIDs[i] == 0) {
            //Envoy process
            for (int j = 0; j < maester->envoys; j++) {
                //Close other pipes
                if (j != i) {
                    close(maester->envoyPInfo.p2c[j][0]);
                    close(maester->envoyPInfo.p2c[j][1]);
                    close(maester->envoyPInfo.c2p[j][0]);
                    close(maester->envoyPInfo.c2p[j][1]);
                }
                //Close own pipes we don't use
                close(maester->envoyPInfo.p2c[i][1]);
                close(maester->envoyPInfo.c2p[i][0]);
            }

            Envoy envoy;
            envoy.p2c = maester->envoyPInfo.p2c[i][0];
            envoy.c2p = maester->envoyPInfo.c2p[i][1];
    
            destroyEnvoys(maester);
            destroyMaester(maester);
            envoyProcess(envoy);
        } else if (maester->envoyPInfo.envoyPIDs[i] > 0) {
            //Maester process
            close(maester->envoyPInfo.p2c[i][0]);
            close(maester->envoyPInfo.c2p[i][1]);
        } else {
            customWrite(1, RED "ERROR | Fork failed\n" RESET);
            destroyMaester(maester);
            return;
        }
    }
}