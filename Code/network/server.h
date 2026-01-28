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



//Project libraries
#include "utils.h"
#include "dataStructures.h"

void *serverThread(void *arg);

#endif