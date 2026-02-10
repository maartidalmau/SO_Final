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
Aquestes 4 funcions d'abaix permeten enviar i rebre dades entre el maester i el envoy.
*/

void passDataToEnvoy(Maester *maester, int envoyIndex, char *data) {
    customWrite(maester->envoyPInfo.p2c[envoyIndex][1], data);
}

void getDataFromEnvoy(Maester *maester, int envoyIndex, char **buffer) {
    customRead(maester->envoyPInfo.c2p[envoyIndex][0], buffer, '\n');
}

void passDataToMaester(Envoy envoy, char *data) {
    customWrite(envoy.c2p, data);
}

void getDataFromMaester(Envoy envoy, char **buffer) {
    customRead(envoy.p2c, buffer, '\n');
/*
Aquestes funcions crean i destruixene memoria compartida per envoysAvailable, s'ha de moure al maester i cridarla al iniciar el maester
*/

int reserveMemoryEnvoysAvailable(Maester *maester) {
    int memid = shmget(IPC_PRIVATE, sizeof(int) * maester->envoys, IPC_CREAT | IPC_EXCL| 0600);
    if (memid < 0) {
        return memid;
    }

    maester->envoysAvailable = shmat(memid, NULL, 0);
    if (maester->envoysAvailable == (void*)-1) {
        return -1;
}

void destroyEnvoysAvailable(Maester *maester) {
    if (maester->envoysAvailable) {
        shmdt((void*)maester->envoysAvailable);
        maester->envoysAvailable = NULL;
    }
}