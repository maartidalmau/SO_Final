#include "server.h"
#include "frame.h"
#include "frameHandler.h"

#include <errno.h>
#include <sys/time.h>

void *workerThread(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;
    
    Frame frame;
    if (receiveFrame(args->clientSocket, &frame) < 0) {
        customWrite(1, RED "Error [RECEIVE_FAILED]\n" RESET);
        close(args->clientSocket);
        free(args);
        return NULL;
    }
    
    if (!validateChecksum(&frame)) {
        sendNack(args->clientSocket, args->maester->name, "CHECKSUM");
        close(args->clientSocket);
        free(args);
        return NULL;
    }
    
    processFrame(args->maester, &frame, args->clientSocket);
    
    close(args->clientSocket);
    free(args);
    
    return NULL;
}

void *serverThread(void *arg) {
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

    int reuseAddr = 1;
    setsockopt(maester->serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr));

    //Bind server socket
    if (bind(maester->serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        char *msg;
        asprintf(&msg, RED "ERROR | Cannot bind socket options (%s)\n" RESET, strerror(errno));
        customWrite(1, msg);
        free(msg);
        close(maester->serverSocket);
        maester->running = 0;
        return NULL;
    }

    //Listen on server socket
    if (listen(maester->serverSocket, 10) < 0) {
        char *msg;
        close(maester->serverSocket);
        asprintf(&msg, RED "ERROR | Cannot listen on server socket (%s)\n" RESET, strerror(errno));
        customWrite(1, msg);
        free(msg);
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
        
        // Timeout de lectura: una connexió entrant que es queda a mitges
        struct timeval rcvTimeout;
        rcvTimeout.tv_sec = 125;
        rcvTimeout.tv_usec = 0;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout));

        WorkerArgs *workerArgs = malloc(sizeof(WorkerArgs));

        workerArgs->clientSocket = clientSocket;
        workerArgs->maester = maester;
        workerArgs->clientAddr = clientAddr;
        
        pthread_t workerThreadID;
        
        if (pthread_create(&workerThreadID, NULL, workerThread, workerArgs) != 0) {
            customWrite(1, RED "ERROR | Cannot create worker thread\n" RESET);
            close(clientSocket);
            free(workerArgs);
        } else {
            pthread_detach(workerThreadID);
        }

    }

    if (maester->serverSocket >= 0) {
        close(maester->serverSocket);
        maester->serverSocket = -1;
    }
    return NULL;
}
