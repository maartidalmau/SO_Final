#include "allianceHandler.h"


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

Alliance* findAlliance(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return NULL;
    }
    
    Alliance *result = NULL;
    
    pthread_mutex_lock(&maester->alliances_mutex);
    
    for (int i = 0; i < maester->numAlliances; i++) {
        if (strcasecmp(maester->alliances[i].name, realmName) == 0) {
            result = &maester->alliances[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&maester->alliances_mutex);
    
    return result;
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
    if (!temp) {
        pthread_mutex_unlock(&maester->alliances_mutex);
        customWrite(1, RED "ERROR | Cannot allocate memory for alliance\n" RESET);
        return;
    }
    
    maester->alliances = temp;
    
    // Inicializar nueva alianza
    maester->alliances[maester->numAlliances].name = strdup(name);
    maester->alliances[maester->numAlliances].ip = ip ? strdup(ip) : NULL;
    maester->alliances[maester->numAlliances].port = port;
    maester->alliances[maester->numAlliances].status = status;
    maester->alliances[maester->numAlliances].requestTime = time(NULL);  // Timestamp actual
    
    maester->numAlliances++;
    
    pthread_mutex_unlock(&maester->alliances_mutex);
}
