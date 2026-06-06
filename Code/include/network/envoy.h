#ifndef ENVOY_H
#define ENVOY_H

#include <sys/wait.h>
#include <stdio.h>

#include "dataStructures.h"
#include "ipc.h"

void createEnvoys(Maester *maester);
int reserveEnvoy(Maester *maester);
void releaseEnvoy(Maester *maester, int envoyIndex);
int dispatchEnvoyRequest(Maester *maester, int envoyIndex, const IpcRequest *request, IpcResponse *response);
void setEnvoyMission(Maester *maester, int envoyIndex, const char *mission);
void releaseEnvoyMissionForRealm(Maester *maester, const char *realmName, const char *missionPrefix);

#endif
