#include "envoy.h"
#include "dataStructures.h"


void initEnvoys(Maester *maester){
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
            return 0;
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
}

void destroyEnvoys(Maester *maester){
    for(int i=0; i<maester->envoys;i++){
        wait();
    }
}