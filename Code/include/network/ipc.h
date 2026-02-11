#ifndef IPC_H
#define IPC_H

#include "dataStructures.h"

void passDataToEnvoy(Maester *maester, int envoyIndex, char *data);

void getDataFromEnvoy(Maester *maester, int envoyIndex, char **buffer);

void passDataToMaester(Envoy envoy, char *data);

void getDataFromMaester(Envoy envoy, char **buffer);