#ifndef SOF1_SERVER_H
#define SOF1_SERVER_H

//Standard libraries
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>

//Project libraries
#include "utils.h"
#include "dataStructures.h"

/**
 * Estructura para pasar argumentos a los worker threads
 */
typedef struct {
    int clientSocket;
    Maester *maester;
    struct sockaddr_in clientAddr;
} WorkerArgs;

/**
 * Thread principal del servidor que acepta conexiones
 */
void *serverThread(void *arg);

/**
 * Worker thread que procesa cada conexión entrante
 */
void *workerThread(void *arg);

#endif