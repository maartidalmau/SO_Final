#include "server.h"
#include "frame.h"
#include "frameHandler.h"

void *workerThread(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;
    
    // Get client IP for logging
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(args->clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
    
    char *msg;
    asprintf(&msg, CYAN "INFO | Connection from %s:%d\n" RESET, clientIP, ntohs(args->clientAddr.sin_port));
    customWrite(1, msg);
    free(msg);
    
    // 1. Recibir frame
    Frame frame;
    if (receiveFrame(args->clientSocket, &frame) < 0) {
        customWrite(1, RED "Els corbs s'han perdut - Error [RECEIVE_FAILED]\n" RESET);
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
    
    // 3. Imprimir diagnóstico
    asprintf(&msg, MAGENTA "Frame received: TYPE=0x%02X FROM=[%s] TO=[%s] DATA_LEN=%d\n" RESET,
             frame.type, frame.ip_origin, frame.ip_destination, frame.data_lenght);
    customWrite(1, msg);
    free(msg);
    
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
    if (maester->serverSocket < 0) {
        customWrite(1, RED "ERROR | Cannot create server socket\n" RESET);
        maester->running = 0;
        return NULL;
    }

    //Bind server socket
    if (bind(maester->serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        close(maester->serverSocket);
        customWrite(1, RED "ERROR | Cannot bind server socket\n" RESET);
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

    char *msg;
    asprintf(&msg, GREEN "INFO | Server listening on %s:%d\n" RESET, maester->ip, maester->port);
    customWrite(1, msg);
    free(msg);

    while (maester->running) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        //Accept client connection
        int clientSocket = accept(maester->serverSocket, (struct sockaddr *)&clientAddr, &clientLen);

        if (clientSocket < 0) {
            if (maester->running) {
                customWrite(1, RED "ERROR | Cannot accept client connection\n" RESET);
            }
            continue;
        }
        
        // Crear estructura de argumentos para el worker
        WorkerArgs *workerArgs = malloc(sizeof(WorkerArgs));
        if (!workerArgs) {
            customWrite(1, RED "ERROR | Cannot allocate memory for worker\n" RESET);
            close(clientSocket);
            continue;
        }
        
        workerArgs->clientSocket = clientSocket;
        workerArgs->maester = maester;
        workerArgs->clientAddr = clientAddr;
        
        // Crear worker thread detached (se limpia solo)
        pthread_t workerThreadID;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        
        if (pthread_create(&workerThreadID, &attr, workerThread, workerArgs) != 0) {
            customWrite(1, RED "ERROR | Cannot create worker thread\n" RESET);
            close(clientSocket);
            free(workerArgs);
        }
        
        pthread_attr_destroy(&attr);
    }

    asprintf(&msg, YELLOW "INFO | Shutting down server on %s:%d\n" RESET, maester->ip, maester->port);
    customWrite(1, msg);
    free(msg);

    close(maester->serverSocket);
    return NULL;
}