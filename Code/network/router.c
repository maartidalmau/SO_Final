#include "router.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>

int isDestination(Maester *maester, const char *destination) {
    if (!maester || !destination) {
        return 0;
    }
    
    return (strcmp(maester->name, destination) == 0);
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
    
    // 1. Apply port +2 rule
    int targetPort = port + 2;
    
    // 2. Print diagnostic message based on port parity
    if (targetPort % 2 == 0) {
        customWrite(1, "Even-port route engaged\n");
    } else {
        customWrite(1, "Odd-port route engaged\n");
    }
    
    // 3. Create TCP socket
    int raven_fd_client = socket(AF_INET, SOCK_STREAM, 0);
    if (raven_fd_client < 0) {
        return -1;
    }
    
    // 4. Configure server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        close(raven_fd_client);
        return -1;
    }
    
    serverAddr.sin_port = htons(targetPort);
    
    // 5. Connect to server
    if (connect(raven_fd_client, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        close(raven_fd_client);
        return -1;
    }
    
    // 6. Return socket descriptor
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
    if (!maester || !frame) {
        return -1;
    }
    
    // ═══════════════════════════════════════════════════════════
    // STEP 1: Find route to destination
    // ═══════════════════════════════════════════════════════════
    
    Route *nextHop = findRoute(maester, frame->ip_destination);
    
    if (!nextHop) {
        // No direct route, try DEFAULT
        nextHop = getDefaultRoute(maester);
    }
    
    if (!nextHop) {
        // No DEFAULT either - cannot forward
        customWrite(1, RED "ERROR | No route to destination\n" RESET);
        
        // Send NACK to sender according to protocol:
        // ORIGIN: Empty, DESTINATION: Empty, DATA: RealmName
        //Frame nackFrame;
        //createNACKFrame(&nackFrame, maester->name);
        //sendFrame(fromSocket, &nackFrame);
        
        return -1;
    }
    
    // ═══════════════════════════════════════════════════════════
    // STEP 2: Connect to next hop
    // ═══════════════════════════════════════════════════════════
    
    int raven_fd_hop;
    if (connectToRealm(nextHop, &raven_fd_hop) < 0) {
        customWrite(1, RED "ERROR | Cannot connect to next hop\n" RESET);
        
        // Send NACK to sender according to protocol
        //Frame nackFrame;
        //createNACKFrame(&nackFrame, maester->name);
        //sendFrame(fromSocket, &nackFrame);
        
        return -1;
    }
    
    // ═══════════════════════════════════════════════════════════
    // STEP 3: Forward frame WITHOUT modification
    // ═══════════════════════════════════════════════════════════
    
    // IMPORTANT: Frame is forwarded as-is, no changes to ORIGIN or DESTINATION
    if (sendFrame(raven_fd_hop, frame) < 0) {
        close(raven_fd_hop);
        
        // Send NACK to sender according to protocol
        //Frame nackFrame;
        //createNACKFrame(&nackFrame, maester->name);
        //sendFrame(fromSocket, &nackFrame);
        
        return -1;
    }
    
    // ═══════════════════════════════════════════════════════════
    // STEP 4: Send ACK to sender (hop confirmation)
    // ═══════════════════════════════════════════════════════════
    
    //Frame ackFrame;
    //createACKFrame(&ackFrame, maester->name, frame->ip_origin);
    //sendFrame(fromSocket, &ackFrame);
    
    // ═══════════════════════════════════════════════════════════
    // STEP 5: Close connection and finish
    // ═══════════════════════════════════════════════════════════
    
    close(raven_fd_hop);
    
    return 0;
}
