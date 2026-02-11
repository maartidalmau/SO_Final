#include "ipc.h"
#include <sys/types.h>
#include <time.h>


// Envía inventario y alianzas al Envoy (protegido por semáforo)
void passDataToEnvoy(Maester *maester, int envoyIndex) {
    SEM_wait(&maester->modifyMaesterData);
    
    uint8_t *buffer = malloc(sizeof(Maester));
    int serializedSize = serializeMaesterData(maester, buffer, sizeof(Maester));
    
    uint32_t size = (uint32_t)serializedSize;
    write(maester->envoyPInfo.p2c[envoyIndex][1], &size, sizeof(uint32_t));
    write(maester->envoyPInfo.p2c[envoyIndex][1], buffer, serializedSize);
    
    free(buffer);
}

// Lee datos modificados del Envoy (inventario y alianzas)
void getDataFromEnvoy(Maester *maester, int envoyIndex, Maester *receivedData) {
    uint32_t size;
    read(maester->envoyPInfo.c2p[envoyIndex][0], &size, sizeof(uint32_t));
    
    uint8_t *buffer = malloc(size);
    read(maester->envoyPInfo.c2p[envoyIndex][0], buffer, size);
    
    deserializeMaesterData(buffer, size, receivedData);
    SEM_signal(&maester->modifyMaesterData);
    free(buffer);
}

// Envía datos al Maester desde el Envoy (inventario y alianzas modificados)
void passDataToMaester(Envoy envoy, Maester *maester) {
    uint8_t *buffer = malloc(sizeof(Maester));
    int serializedSize = serializeMaesterData(maester, buffer, sizeof(Maester));
    
    uint32_t size = (uint32_t)serializedSize;
    write(envoy.c2p, &size, sizeof(uint32_t));
    write(envoy.c2p, buffer, serializedSize);
    
    free(buffer);
}

// Recibe datos del Envoy en el Maester
void getDataFromMaester(Envoy envoy, Maester *receivedData) {
    uint32_t size;
    read(envoy.p2c, &size, sizeof(uint32_t));
    
    uint8_t *buffer = malloc(size);
    read(envoy.p2c, buffer, size);
    
    deserializeMaesterData(buffer, size, receivedData);
    free(buffer);
}

// SERIALIZACIÓN DE DATOS DEL MAESTER (solo cantidad de productos e status de alianzas)
int serializeMaesterData(Maester *maester, uint8_t *buffer, int bufferSize) {
    
    int offset = 0;
    memset(buffer, 0, bufferSize);
    
    memcpy(buffer + offset, &maester->numProducts, sizeof(int));
    offset += sizeof(int);
    
    for (int i = 0; i < maester->numProducts && offset + 4 < bufferSize; i++) {
        memcpy(buffer + offset, &maester->inventory[i].amount, sizeof(int));
        offset += sizeof(int);
    }
    
    memcpy(buffer + offset, &maester->numAlliances, sizeof(int));
    offset += sizeof(int);
    
    //solo las alianzas
    for (int i = 0; i < maester->numAlliances && offset + 4 < bufferSize; i++) {
        memcpy(buffer + offset, &maester->alliances[i].status, sizeof(int));
        offset += sizeof(int);
    }
    
    return offset;
}

int deserializeMaesterData(uint8_t *buffer, int bufferSize, Maester *maester) {
    int offset = 0;
    
    memcpy(&maester->numProducts, buffer + offset, sizeof(int));
    offset += sizeof(int);
    
    for (int i = 0; i < maester->numProducts && offset + 4 <= bufferSize; i++) {
        int newAmount;
        memcpy(&newAmount, buffer + offset, sizeof(int));
        maester->inventory[i].amount = newAmount;
        offset += sizeof(int);
    }
    
    memcpy(&maester->numAlliances, buffer + offset, sizeof(int));
    offset += sizeof(int);
    
    for (int i = 0; i < maester->numAlliances && offset + 4 <= bufferSize; i++) {
        int newStatus;
        memcpy(&newStatus, buffer + offset, sizeof(int));
        maester->alliances[i].status = newStatus;
        offset += sizeof(int);
    }
    
    return offset;
}