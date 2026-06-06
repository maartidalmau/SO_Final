#ifndef CLIENT_H
#define CLIENT_H

#define _GNU_SOURCE

#include "frame.h"
#include "router.h"
#include "dataStructures.h"
#include "allianceHandler.h"
#include "ipc.h"


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>


void notifyDisconnect(Maester *maester);

int sendAllianceRequest(Maester *maester, const char *realmName, const char *sigilPath);

int sendAllianceResponse(Maester *maester, const char *realmName, int accept);

int sendProductListRequest(Maester *maester, const char *realmName);

int envoySendAllianceRequest(const IpcRequest *request, IpcResponse *response);

int envoySendAllianceResponse(const IpcRequest *request, IpcResponse *response);

int envoySendProductListRequest(const IpcRequest *request, IpcResponse *response);

#endif // CLIENT_H
