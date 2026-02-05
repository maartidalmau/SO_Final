#include "envoy.h"

void initEnvoys(Maester *maester){
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
            exit(0);
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

void destroyEnvoys(Maester *maester){
    for(int i = 0; i<maester->envoys;i++){
        kill(maester->envoyPInfo.envoyPIDs[i], SIGUSR1);
    }

    for (int i = 0; i < maester->envoys; i++) {
        waitpid(maester->envoyPInfo.envoyPIDs[i], NULL, 0);
        free(maester->envoyPInfo.p2c[i]);
        free(maester->envoyPInfo.c2p[i]);
    }

    free(maester->envoyPInfo.p2c);
    free(maester->envoyPInfo.c2p);
    free(maester->envoyPInfo.envoyPIDs);
}