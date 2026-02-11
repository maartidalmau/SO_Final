#include "ipc.h"

void passDataToEnvoy(Maester *maester, int envoyIndex, char *data) {
    customWrite(maester->envoyPInfo.p2c[envoyIndex][1], data);
}

void getDataFromEnvoy(Maester *maester, int envoyIndex, char **buffer) {
    customRead(maester->envoyPInfo.c2p[envoyIndex][0], buffer, '\n');
}

void passDataToMaester(Envoy envoy, char *data) {
    customWrite(envoy.c2p, data);
}

void getDataFromMaester(Envoy envoy, char **buffer) {
    customRead(envoy.p2c, buffer, '\n');
}