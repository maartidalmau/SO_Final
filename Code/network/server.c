#include "server.h"

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

        if (clientSocket < 0 && maester->running) {
            customWrite(1, RED "ERROR | Cannot accept client connection\n" RESET);
            continue;
        }
    }

    asprintf(&msg, YELLOW "INFO | Shutting down server on %s:%d\n" RESET, maester->ip, maester->port);
    customWrite(1, msg);
    free(msg);

    close(maester->serverSocket);
    return NULL;
}