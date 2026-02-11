/*
Aquesta funció buscara qualsevol envoy que estigui lliure i el reservara, aixi el maester el pot utilitzar. Si no hi ha cap 
envoy disponible, fa un return de 0 i ja posteriorment mostrarem un error amb el maester
*/

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

/*
Aquestes funcions crean i destruixene memoria compartida per envoysAvailable, s'ha de moure al maester i cridarla al iniciar el maester
*/

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