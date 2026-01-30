#include "client.h"

int connectToRealmWithEnvoy(Maester *maester, const char *realmName, int *raven_fd_out) {
    if (!maester || !realmName || !raven_fd_out) {
        return -1;
    }
    
    char *msg;
    
    //Adquirir envoy (bloquea si todos ocupados)
    asprintf(&msg, CYAN "Waiting for available envoy...\n" RESET);
    customWrite(1, msg);
    free(msg);
    
    sem_wait(&maester->envoys_sem);  // Decrementa contador, bloquea si es 0
    
    asprintf(&msg, GREEN "Envoy acquired! Connecting to [%s]...\n" RESET, realmName);
    customWrite(1, msg);
    free(msg);
    
    //Buscar ruta al reino
    Route *route = findRoute(maester, realmName);
    if (!route) {
        route = getDefaultRoute(maester);
    }
    
    if (!route) {
        asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        
        sem_post(&maester->envoys_sem);  // Liberar envoy antes de salir
        return -1;
    }
    
    // Conectar al reino
    if (connectToRealm(route, raven_fd_out) < 0) {
        asprintf(&msg, RED "ERROR | Cannot connect to realm [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        
        sem_post(&maester->envoys_sem);  // Liberar envoy antes de salir
        return -1;
    }
    
    asprintf(&msg, GREEN "Connected to [%s] successfully\n" RESET, realmName);
    customWrite(1, msg);
    free(msg);
    
    return 0;  // Éxito - NO liberar envoy aún (se libera después de la comunicación)
}

// ═══════════════════════════════════════════════════════════
// STUBS: Funciones pendientes de implementar
// ═══════════════════════════════════════════════════════════

int sendAllianceRequest(Maester *maester, const char *realmName, const char *sigilPath) {
    (void)maester;
    (void)realmName;
    (void)sigilPath;
    
    customWrite(1, YELLOW "TODO: sendAllianceRequest not yet implemented\n" RESET);
    return -1;
}

int sendAllianceResponse(Maester *maester, const char *realmName, int accept) {
    (void)maester;
    (void)realmName;
    (void)accept;
    
    customWrite(1, YELLOW "TODO: sendAllianceResponse not yet implemented\n" RESET);
    return -1;
}

int sendProductListRequest(Maester *maester, const char *realmName) {
    (void)maester;
    (void)realmName;
    
    customWrite(1, YELLOW "TODO: sendProductListRequest not yet implemented\n" RESET);
    return -1;
}
