#include "server.h"
#include "frame.h"
#include "frameHandler.h"

void *workerThread(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;
    
    Frame frame;
    if (receiveFrame(args->clientSocket, &frame) < 0) {
        customWrite(1, RED "Error [RECEIVE_FAILED]\n" RESET);
        close(args->clientSocket);
        free(args);
        return NULL;
    }
    
    // 2. Validar checksum
    if (!validateChecksum(&frame)) {
        sendNack(args->clientSocket, args->maester->name, "CHECKSUM");
        close(args->clientSocket);
        free(args);
        return NULL;
    }
    
    // 4. Procesar frame (dispatcher)
    processFrame(args->maester, &frame, args->clientSocket);
    
    // 5. Cerrar conexión
    close(args->clientSocket);
    free(args);
    
    return NULL;
}

void *serverThread(void *arg) {
    //Obtain maester struct
    Maester *maester = (Maester *)arg;

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, maester->ip, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(maester->port);

    //Create server socket
    maester->serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    //Bind server socket
    if (bind(maester->serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        customWrite(1, RED "ERROR | Cannot bind socket options\n" RESET);
        close(maester->serverSocket);
        maester->running = 0;
        return NULL;
    }

    //Listen on server socket
    if (listen(maester->serverSocket, 10) < 0) {
        close(maester->serverSocket);
        customWrite(1, RED "ERROR | Cannot listen on server socket\n" RESET);
        maester->running = 0;
        return NULL;
    }

    while (maester->running) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        int clientSocket = accept(maester->serverSocket, (struct sockaddr *)&clientAddr, &clientLen);

        if (clientSocket < 0) {
            if (maester->running) {
                customWrite(1, RED "ERROR | Cannot accept client connection\n" RESET);
            }
            continue;
        }
        
        WorkerArgs *workerArgs = malloc(sizeof(WorkerArgs));
        
        workerArgs->clientSocket = clientSocket;
        workerArgs->maester = maester;
        workerArgs->clientAddr = clientAddr;
        
        pthread_t workerThreadID;
        
        if (pthread_create(&workerThreadID, NULL, workerThread, workerArgs) != 0) {
            customWrite(1, RED "ERROR | Cannot create worker thread\n" RESET);
            close(clientSocket);
            free(workerArgs);
        }

    }

    close(maester->serverSocket);
    return NULL;
}

void cerrarWorkers(Maester *maester){
    pthread_mutex_lock(&maester->workersInfo->workers_mutex);
    for (int i = 0; i < maester->workersInfo->numWorkers; i++){
        pthread_join(maester->workersInfo->workersThreadID[i], NULL);
    }
    pthread_mutex_unlock(&maester->workersInfo->workers_mutex);
}