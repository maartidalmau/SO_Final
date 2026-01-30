#include "router.h"


int isDestination(Maester *maester, const char *destination) {
    if (!maester || !destination) {
        return 0;
    }
    
    return (strcasecmp(maester->name, destination) == 0);
}

Route* findRoute(Maester *maester, const char *realmName) {
    if (!maester || !realmName) {
        return NULL;
    }
    
    Route *result = NULL;
    
    // Lock: protect access to routes array
    pthread_mutex_lock(&maester->routes_mutex);
    
    for (int i = 0; i < maester->numRoutes; i++) {
        // Skip DEFAULT route (use getDefaultRoute() for that)
        if (strcasecmp(maester->routes[i].name, "DEFAULT") == 0) {
            continue;
        }
        
        // Case-insensitive comparison
        if (strcasecmp(maester->routes[i].name, realmName) == 0) {
            result = &maester->routes[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&maester->routes_mutex);
    
    return result;
}

Route* getDefaultRoute(Maester *maester) {
    if (!maester) {
        return NULL;
    }
    
    Route *result = NULL;
    
    // Lock: protect access to routes array
    pthread_mutex_lock(&maester->routes_mutex);
    
    for (int i = 0; i < maester->numRoutes; i++) {
        if (strcasecmp(maester->routes[i].name, "DEFAULT") == 0) {
            result = &maester->routes[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&maester->routes_mutex);
    
    return result;
}

int connectToRealmByRoute(const char *ip, int port, int *raven_fd_out) {
    if (!ip || !raven_fd_out) {
        return -1;
    }
    
    // Use the port directly (no +2 rule)
    int targetPort = port;
    
    char *msg;
    asprintf(&msg, CYAN "Connecting to %s:%d...\n" RESET, ip, targetPort);
    customWrite(1, msg);
    free(msg);
    
    // Create TCP socket
    int raven_fd_client = socket(AF_INET, SOCK_STREAM, 0);
    if (raven_fd_client < 0) {
        customWrite(1, RED "Els corbs s'han perdut - Error [SOCKET_CREATE_FAILED]\n" RESET);
        return -1;
    }
    
    // Configure server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        customWrite(1, RED "ERROR | Invalid IP address\n" RESET);
        close(raven_fd_client);
        return -1;
    }
    
    serverAddr.sin_port = htons(targetPort);
    
    // Connect to server
    if (connect(raven_fd_client, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        asprintf(&msg, RED "ERROR | Connection refused to %s:%d\n" RESET, ip, targetPort);
        customWrite(1, msg);
        free(msg);
        close(raven_fd_client);
        return -1;
    }
    
    asprintf(&msg, GREEN "Connected successfully to %s:%d\n" RESET, ip, targetPort);
    customWrite(1, msg);
    free(msg);
    
    // Return socket descriptor
    *raven_fd_out = raven_fd_client;
    return 0;
}

int connectToRealm(Route *route, int *raven_fd_out) {
    if (!route || !raven_fd_out) {
        return -1;
    }
    
    return connectToRealmByRoute(route->ip, route->port, raven_fd_out);
}

int forwardFrame(Maester *maester, Frame *frame, int fromSocket) {
    (void)fromSocket; //De moment no utilitzem aquesta funcio
    if (!maester || !frame) {
        return -1;
    }
    
    if (strcasecmp(maester->name, frame->ip_origin) == 0) {
        // We sent this frame originally, and it came back to us
        customWrite(1, RED "Els corbs s'han perdut - Error [LOOP_DETECTED]\n" RESET);        
        return -1;
    }
    
    Route *nextHop = findRoute(maester, frame->ip_destination);
    
    if (!nextHop) {
        // No direct route, try DEFAULT
        nextHop = getDefaultRoute(maester);
    }
    
    if (!nextHop) {
        // No DEFAULT either - cannot forward
        sendNack(fromSocket, maester->name, "NO_ROUTE");
        return -1;
    }
    
    int raven_fd_hop;
    if (connectToRealm(nextHop, &raven_fd_hop) < 0) {
        sendNack(fromSocket, maester->name, "CONNECT_FAILED");
        return -1;
    }
    
    // IMPORTANT: Frame is forwarded as-is, no changes to ORIGIN or DESTINATION
    if (sendFrame(raven_fd_hop, frame) < 0) {
        close(raven_fd_hop);
        sendNack(fromSocket, maester->name, "SEND_FAILED");
        return -1;
    }
    
    char *msg;
    asprintf(&msg, GREEN "Frame forwarded successfully to [%s]\n" RESET, nextHop->name);
    customWrite(1, msg);
    free(msg);
    
    // ═══════════════════════════════════════════════════════════
    // STEP 4: Send ACK to sender (hop confirmation)
    // ═══════════════════════════════════════════════════════════
    
    //Frame ackFrame;
    //createACKFrame(&ackFrame, maester->name, frame->ip_origin);
    //sendFrame(fromSocket, &ackFrame);
    
    close(raven_fd_hop);
    
    return 0;
}
