#include "router.h"

#include <sys/select.h>

static int writeAllBytes(int fd, const uint8_t *buf, unsigned long n) {
    unsigned long total = 0;
    while (total < n) {
        long w = write(fd, buf + total, n - total);
        if (w <= 0) {
            return -1;
        }
        total += (unsigned long)w;
    }
    return 0;
}

// Relay transparent bidireccional entre dues connexions fins que una es tanca.
// Converteix el node intermedi en una tuberia: així les comunicacions
// origen<->destí (handshake d'aliança, transferència de fitxers, PING...)
// travessen els hops com si fos una connexió directa, sense modificar les
// trames. Encadena de forma recursiva amb múltiples salts.
static void relayBidirectional(int a, int b) {
    uint8_t buf[1024];
    int maxfd = (a > b ? a : b) + 1;

    while (1) {
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(a, &readfds);
        FD_SET(b, &readfds);
        tv.tv_sec = 125;   // > 2 min (timeout del protocol); evita relays penjats
        tv.tv_usec = 0;

        int sel = select(maxfd, &readfds, NULL, NULL, &tv);
        if (sel <= 0) {
            break;  // timeout o error: tanquem el relay
        }

        if (FD_ISSET(a, &readfds)) {
            long n = read(a, buf, sizeof(buf));
            if (n <= 0 || writeAllBytes(b, buf, (unsigned long)n) < 0) {
                break;
            }
        }
        if (FD_ISSET(b, &readfds)) {
            long n = read(b, buf, sizeof(buf));
            if (n <= 0 || writeAllBytes(a, buf, (unsigned long)n) < 0) {
                break;
            }
        }
    }
}

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
    
    // First, try to find the specific realm
    for (int i = 0; i < maester->numRoutes; i++) {
        // Skip DEFAULT route in this first pass
        if (strcasecmp(maester->routes[i].name, "DEFAULT") == 0) {
            continue;
        }
        
        // Case-insensitive comparison
        if (strcasecmp(maester->routes[i].name, realmName) == 0) {
            result = &maester->routes[i];
            break;
        }
    }
    
    // If not found, try DEFAULT route as fallback
    if (!result) {
        for (int i = 0; i < maester->numRoutes; i++) {
            if (strcasecmp(maester->routes[i].name, "DEFAULT") == 0) {
                result = &maester->routes[i];
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&maester->routes_mutex);
    
    return result;
}

int getRouteInfo(Maester *maester, const char *realmName, char **ipOut, int *portOut) {
    if (!maester) {
        return 0;
    }
    
    int found = 0;
    int searchDefault = (realmName == NULL);
    
    pthread_mutex_lock(&maester->routes_mutex);
    
    for (int i = 0; i < maester->numRoutes; i++) {
        int isDefault = (strcasecmp(maester->routes[i].name, "DEFAULT") == 0);
        
        if (searchDefault) {
            // Looking for DEFAULT route
            if (isDefault) {
                if (ipOut) *ipOut = maester->routes[i].ip ? strdup(maester->routes[i].ip) : NULL;
                if (portOut) *portOut = maester->routes[i].port;
                found = 1;
                break;
            }
        } else {
            // Looking for specific realm (skip DEFAULT)
            if (isDefault) continue;
            
            if (strcasecmp(maester->routes[i].name, realmName) == 0) {
                if (ipOut) *ipOut = maester->routes[i].ip ? strdup(maester->routes[i].ip) : NULL;
                if (portOut) *portOut = maester->routes[i].port;
                found = 1;
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&maester->routes_mutex);
    
    return found;
}

int connectToRealmByRoute(const char *ip, int port, int *fd_out) {
    if (!ip || !fd_out) {
        return -1;
    }
    
    int targetPort = port;

    char *msg;

    // Create TCP socket
    int fd_client = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_client < 0) {
        customWrite(1, RED "Error [SOCKET_CREATE_FAILED]\n" RESET);
        return -1;
    }
    
    // Configure server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, ip, &serverAddr.sin_addr) <= 0) {
        customWrite(1, RED "ERROR | Invalid IP address\n" RESET);
        close(fd_client);
        return -1;
    }
    
    serverAddr.sin_port = htons(targetPort);
    
    // Connect to server
    if (connect(fd_client, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        asprintf(&msg, RED "ERROR | Connection refused to %s:%d\n" RESET, ip, targetPort);
        customWrite(1, msg);
        free(msg);
        close(fd_client);
        return -1;
    }
    
    // Return socket descriptor
    *fd_out = fd_client;
    return 0;
}

int connectToRealm(Route *route, int *fd_out) {
    if (!route || !fd_out) {
        return -1;
    }
    
    return connectToRealmByRoute(route->ip, route->port, fd_out);
}

int forwardFrame(Maester *maester, Frame *frame, int fromSocket) {
    if (!maester || !frame) {
        return -1;
    }
    char myIpPort[IP_SIZE];
    snprintf(myIpPort, IP_SIZE, "%s:%d", maester->ip, maester->port);
    
    if (strcmp(myIpPort, frame->ip_origin) == 0) {
        customWrite(1, RED "Error [LOOP_DETECTED]\n" RESET);        
        return -1;
    }
    
    Route *nextHop = findRoute(maester, frame->ip_destination);
    
    if (!nextHop) {
        // Enviar ERR_UNKNOWN_REALM (0x21) al origen
        char myIpPort[IP_SIZE];
        snprintf(myIpPort, IP_SIZE, "%s:%d", maester->ip, maester->port);
        
        char errorData[DATA_MAX_SIZE];
        snprintf(errorData, DATA_MAX_SIZE, "UNKNOWN_REALM&%s", frame->ip_destination);
        
        Frame errorFrame;
        createFrame(&errorFrame, ERR_UNKNOWN_REALM, myIpPort, frame->ip_origin, errorData);
        
        // Intentar enviar l'error al origen (si tenim ruta)
        Route *originRoute = findRoute(maester, frame->ip_origin);
        
        if (originRoute) {
            int error_fd;
            if (connectToRealm(originRoute, &error_fd) == 0) {
                sendFrame(error_fd, &errorFrame);
                close(error_fd);
            }
        }
        
        char *msg;
        asprintf(&msg, RED "Error [UNKNOWN_REALM: %s]\n" RESET, frame->ip_destination);
        customWrite(1, msg);
        free(msg);
        
        return -1;
    }
    
    int fd_hop;
    if (connectToRealm(nextHop, &fd_hop) < 0) {
        sendNack(fromSocket, maester->name, "CONNECT_FAILED");
        return -1;
    }
    
    if (sendFrame(fd_hop, frame) < 0) {
        close(fd_hop);
        sendNack(fromSocket, maester->name, "SEND_FAILED");
        return -1;
    }

    // La primera trama ja s'ha reexpedit; ara fem de relay transparent per a
    // la resta de la sessió (respostes, ACKs, dades del fitxer, etc.).
    relayBidirectional(fromSocket, fd_hop);

    close(fd_hop);
    return 0;
}
