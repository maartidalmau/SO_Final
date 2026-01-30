#include "client.h"

// ═══════════════════════════════════════════════════════════
// FUNCIÓN BASE: Conectar con gestión de envoys
// ═══════════════════════════════════════════════════════════

int connectToRealmWithEnvoy(Maester *maester, const char *realmName, int *raven_fd_out) {
    if (!maester || !realmName || !raven_fd_out) {
        return -1;
    }
    
    char *msg;
    
    // PASO 1: Adquirir envoy (bloquea si todos ocupados)
    asprintf(&msg, CYAN "Waiting for available envoy...\n" RESET);
    customWrite(1, msg);
    free(msg);
    
    sem_wait(&maester->envoys_sem);  // Decrementa contador, bloquea si es 0
    
    asprintf(&msg, GREEN "Envoy acquired! Connecting to [%s]...\n" RESET, realmName);
    customWrite(1, msg);
    free(msg);
    
    // PASO 2: Buscar ruta al reino
    Route *route = findRoute(maester, realmName);
    if (!route) {
        // No hay ruta directa, intentar DEFAULT
        route = getDefaultRoute(maester);
    }
    
    if (!route) {
        asprintf(&msg, RED "ERROR | No route to realm [%s]\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
        
        sem_post(&maester->envoys_sem);  // Liberar envoy antes de salir
        return -1;
    }
    
    // PASO 3: Conectar al reino
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
// PING: Función simple para verificar conectividad
// ═══════════════════════════════════════════════════════════

int sendPing(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return -1;
    }
    
    // Check if we're trying to ping ourselves
    if (strcasecmp(maester->name, realmName) == 0) {
        char *msg;
        asprintf(&msg, YELLOW "INFO | Cannot ping yourself [%s]\n" RESET, maester->name);
        customWrite(1, msg);
        free(msg);
        return 0;
    }
    
    int raven_fd;
    char *msg;
    
    // 1. Adquirir envoy y conectar
    if (connectToRealmWithEnvoy(maester, realmName, &raven_fd) < 0) {
        return -1;
    }
    
    // 2. Crear frame PING_PONG
    Frame pingFrame;
    createFrame(&pingFrame, PING_PONG, maester->name, realmName, "PING");
    
    // 3. Enviar PING
    asprintf(&msg, CYAN "Sending PING to [%s]...\n" RESET, realmName);
    customWrite(1, msg);
    free(msg);
    
    if (sendFrame(raven_fd, &pingFrame) < 0) {
        customWrite(1, RED "Els corbs s'han perdut - Error [SEND_FAILED]\n" RESET);
        close(raven_fd);
        sem_post(&maester->envoys_sem);  // Liberar envoy
        return -1;
    }
    
    // 4. Recibir PONG
    asprintf(&msg, CYAN "Waiting for PONG from [%s]...\n" RESET, realmName);
    customWrite(1, msg);
    free(msg);
    
    Frame pongFrame;
    if (receiveFrame(raven_fd, &pongFrame) < 0) {
        customWrite(1, RED "Els corbs s'han perdut - Error [RECEIVE_FAILED]\n" RESET);
        close(raven_fd);
        sem_post(&maester->envoys_sem);  // Liberar envoy
        return -1;
    }
    
    // 5. Validar respuesta
    if (pongFrame.type == PING_PONG) {
        asprintf(&msg, GREEN "✓ PONG received from [%s] successfully!\n" RESET, realmName);
        customWrite(1, msg);
        free(msg);
    } else if (pongFrame.type == NACK_ERROR) {
        // NACK format: ORIGIN and DESTINATION empty, DATA contains realm name
        asprintf(&msg, RED "Els corbs s'han perdut - NACK from realm [%s]\n" RESET, pongFrame.data);
        customWrite(1, msg);
        free(msg);
    } else {
        asprintf(&msg, YELLOW "? Unexpected response type: 0x%02X\n" RESET, pongFrame.type);
        customWrite(1, msg);
        free(msg);
    }
    
    // 6. Limpiar recursos
    close(raven_fd);
    sem_post(&maester->envoys_sem);  // Liberar envoy
    
    return 0;
}


void notifyDisconnect(Maester *maester) {
    if (!maester) {
        return;
    }
    
    customWrite(1, YELLOW "Notifying realms of disconnection...\n" RESET);
    
    // Notificar a todos los reinos con alianza activa
    pthread_mutex_lock(&maester->alliances_mutex);
    
    for (int i = 0; i < maester->numAlliances; i++) {
        if (maester->alliances[i].status == 1) {  // Solo activos
            char *msg;
            asprintf(&msg, YELLOW "Notifying [%s]...\n" RESET, maester->alliances[i].name);
            customWrite(1, msg);
            free(msg);
            
            // Intentar conectar y enviar DISCONNECT
            int raven_fd;
            Route *route = findRoute(maester, maester->alliances[i].name);
            
            if (route && connectToRealm(route, &raven_fd) == 0) {
                Frame disconnectFrame;
                createFrame(&disconnectFrame, MAESTER_DISCONNECT, maester->name, 
                           maester->alliances[i].name, "Shutting down");
                
                sendFrame(raven_fd, &disconnectFrame);
                close(raven_fd);
            }
        }
    }
    
    pthread_mutex_unlock(&maester->alliances_mutex);
    
    customWrite(1, GREEN "Disconnect notifications sent\n" RESET);
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
