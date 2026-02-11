#ifndef IPC_H
#define IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dataStructures.h"
#include "utils.h"
#include "network.h"

// Funciones de serialización/deserialización
int serializeMaesterData(Maester *maester, uint8_t *buffer, int bufferSize);
int deserializeMaesterData(uint8_t *buffer, int bufferSize, Maester *maester);

void updateMaesterDataFromEnvoy(Maester *maester, Maester *receivedData);

void passDataToEnvoy(Maester *maester, int envoyIndex);
void getDataFromEnvoy(Maester *maester, int envoyIndex, Maester *receivedData);

void passDataToMaester(Envoy envoy, Maester *maester);
void getDataFromMaester(Envoy envoy, Maester *receivedData);

#endif