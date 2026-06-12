#include "allianceHandler.h"
#include "envoy.h"


int hasAlliance(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return 0;
    }
    
    pthread_mutex_lock(&maester->alliances_mutex);
    
    for (int i = 0; i < maester->numAlliances; i++) {
        if (strcasecmp(maester->alliances[i].name, realmName) == 0 &&
            maester->alliances[i].status == ALLIANCE_ACTIVE) {
            pthread_mutex_unlock(&maester->alliances_mutex);
            return 1;
        }
    }
    
    pthread_mutex_unlock(&maester->alliances_mutex);
    return 0;
}

void addOrUpdateAlliance(Maester *maester, const char *name, const char *ip, int port, int status) {
    if (!maester || !name) {
        return;
    }
    
    pthread_mutex_lock(&maester->alliances_mutex);
    
    // Buscar si ya existe la alianza
    for (int i = 0; i < maester->numAlliances; i++) {
        if (strcasecmp(maester->alliances[i].name, name) == 0) {
            // Actualizar alianza existente
            maester->alliances[i].status = status;
            
            // Actualizar IP si se proporciona
            if (ip) {
                if (maester->alliances[i].ip) {
                    free(maester->alliances[i].ip);
                }
                maester->alliances[i].ip = strdup(ip);
            }
            
            // Actualizar puerto
            if (port > 0) {
                maester->alliances[i].port = port;
            }
            
            // Actualitzar timestamp si és una nova petició PENDING
            if (status == ALLIANCE_PENDING) {
                maester->alliances[i].requestTime = time(NULL);
            }
            
            pthread_mutex_unlock(&maester->alliances_mutex);
            
            return;
        }
    }
    
    // No existe - crear nueva alianza
    Alliance *temp = realloc(maester->alliances, sizeof(Alliance) * (maester->numAlliances + 1));
    
    maester->alliances = temp;
    
    // Inicializar nueva alianza con validación de strdup
    maester->alliances[maester->numAlliances].name = strdup(name);
    maester->alliances[maester->numAlliances].ip = ip ? strdup(ip) : NULL;
    maester->alliances[maester->numAlliances].port = port;
    maester->alliances[maester->numAlliances].status = status;
    maester->alliances[maester->numAlliances].requestTime = time(NULL);

    maester->numAlliances++;
    
    pthread_mutex_unlock(&maester->alliances_mutex);
}

int getAllianceInfo(Maester *maester, const char *realmName, char **ipOut, int *portOut, int *statusOut, time_t *requestTimeOut) {
    if (!maester || !realmName) {
        return 0;
    }

    int found = 0;

    pthread_mutex_lock(&maester->alliances_mutex);

    for (int i = 0; i < maester->numAlliances; i++) {
        if (strcasecmp(maester->alliances[i].name, realmName) == 0) {
            if (ipOut) {
                *ipOut = maester->alliances[i].ip ? strdup(maester->alliances[i].ip) : NULL;
            }
            if (portOut) *portOut = maester->alliances[i].port;
            if (statusOut) *statusOut = maester->alliances[i].status;
            if (requestTimeOut) *requestTimeOut = maester->alliances[i].requestTime;
            found = 1;
            break;
        }
    }

    pthread_mutex_unlock(&maester->alliances_mutex);

    return found;
}


// Revisa les peticions d'aliança PENDING i, si han superat el timeout (2 min)
// sense resposta, marca l'aliança com FAILED i ALLIBERA l'envoy que esperava.
void sweepPledgeTimeouts(Maester *maester) {
    if (!maester) {
        return;
    }

    time_t now = time(NULL);

    // 1) Sota el mutex: recollim els noms dels pledges PENDING caducats (no
    //    canviem l'estat encara per no anidar locks amb releaseEnvoy...).
    char **candidates = NULL;
    int count = 0;
    pthread_mutex_lock(&maester->alliances_mutex);
    for (int i = 0; i < maester->numAlliances; i++) {
        if (maester->alliances[i].status == ALLIANCE_PENDING &&
            difftime(now, maester->alliances[i].requestTime) > ALLIANCE_TIMEOUT_SECONDS) {
            char **tmp = realloc(candidates, sizeof(char *) * (count + 1));
            if (tmp) {
                candidates = tmp;
                candidates[count++] = strdup(maester->alliances[i].name);
            }
        }
    }
    pthread_mutex_unlock(&maester->alliances_mutex);

    // 2) Fora del mutex: si hi havia un envoy nostre en missió (pledge SORTINT),
    //    l'alliberem i marquem l'aliança com FAILED. Si no hi ha envoy (petició
    //    ENTRANT), no la toquem aquí.
    for (int i = 0; i < count; i++) {
        if (releaseEnvoyMissionForRealm(maester, candidates[i], "PLEDGE to ")) {
            addOrUpdateAlliance(maester, candidates[i], NULL, 0, ALLIANCE_FAILED);
            char *msg;
            asprintf(&msg, RED "\n>>> Pledge to %s has failed (TIMEOUT).\n" RESET, candidates[i]);
            customWrite(1, msg);
            free(msg);
            customWrite(1, GREEN "$ " RESET);
        }
        free(candidates[i]);
    }
    free(candidates);
}
