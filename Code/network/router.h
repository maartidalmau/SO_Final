#ifndef ROUTER_H
#define ROUTER_H

#define _GNU_SOURCE

#include "network.h"
#include "frame.h"
#include "dataStructures.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>



int isDestination(Maester *maester, const char *destination);

Route* findRoute(Maester *maester, const char *realmName);

Route* getDefaultRoute(Maester *maester);

int connectToRealmByRoute(const char *ip, int port, int *raven_fd_out);

int connectToRealm(Route *route, int *raven_fd_out);

int forwardFrame(Maester *maester, Frame *frame, int fromSocket);

#endif // ROUTER_H
