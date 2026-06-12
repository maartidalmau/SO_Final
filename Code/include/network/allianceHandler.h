#ifndef ALLIANCE_HANDLER_H
#define ALLIANCE_HANDLER_H

#define _GNU_SOURCE

#include "dataStructures.h"
#include "network.h"


int hasAlliance(Maester *maester, const char *realmName);

void addOrUpdateAlliance(Maester *maester, const char *name, const char *ip, int port, int status);

int getAllianceInfo(Maester *maester, const char *realmName, char **ipOut, int *portOut, int *statusOut, time_t *requestTimeOut);

void sweepPledgeTimeouts(Maester *maester);

#endif // ALLIANCE_HANDLER_H
