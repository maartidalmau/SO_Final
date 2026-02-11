#include "envoy.h"

void envoyProcess(Envoy envoy) {

    //Ignore Ctrl+C
    signal(SIGINT, SIG_IGN);
    
    while(*envoy.running == 1) {
        pause();
    }
    //Close pipes and exit
    close(envoy.p2c);
    close(envoy.c2p);
    exit(0);
}

void createEnvoys(Maester *maester, volatile sig_atomic_t *envoyRunning){
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
            envoy.running = envoyRunning;
    
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

int reserveEnvoy(Maester *maester) {
    int chosenEnvoy = -1;

    //We need to wait for the semaphore to ensure that we are not modifying the envoysAvailable array while it is being read by another thread or process
    SEM_wait(&maester->envoys_sem);

    //Reserve an available envoy
    for (int i = 0; i < maester->envoys; i++) {
        if (maester->envoysAvailable[i] == 1) {
            maester->envoysAvailable[i] = 0;
            chosenEnvoy = i;
            break;
        }
    }

    if (chosenEnvoy != -1) {
        //We signal the semaphore to allow other threads or processes to access the envoysAvailable array
        SEM_signal(&maester->envoys_sem);
        return 1;
    }

    //We signal the semaphore to allow other threads or processes to access the envoysAvailable array
    SEM_signal(&maester->envoys_sem);
    return 0;
}