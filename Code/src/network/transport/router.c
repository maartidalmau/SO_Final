#include "router.h"

#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#define CONNECT_TIMEOUT_SECONDS 10
#define IO_TIMEOUT_SECONDS 125   // > 2 min del protocol; evita lectures penjades

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

// Llegeix EXACTAMENT una trama (320B) d'un fd, gestionant lectures parcials.
// Aquí sí cal muntar la trama sencera (a diferència de receiveFrame, que fa un
// sol read als extrems): el relay treballa sobre un flux de bytes que pot venir
// fragmentat de la xarxa, i ha d'alinear-se a trames per validar-les i reenviar
// -les senceres. Retorna 0 si ha llegit una trama completa, -1 si es tanca/error.
static int relayReadFrame(int fd, uint8_t *buf) {
    long total = 0;
    while (total < TRAMA_SIZE) {
        long n = read(fd, buf + total, TRAMA_SIZE - total);
        if (n <= 0) {
            return -1;
        }
        total += n;
    }
    return 0;
}

// Reenviament trama a trama entre dues connexions fins que una es tanca. El node
// intermedi llegeix cada trama sencera (320B), en VALIDA el checksum (procediment
// d'enrutament de la F2: si és erroni, NACK al hop anterior i descartar) i la
// reenvia sense modificar ORIGIN ni DESTINATION. Encadena amb múltiples salts.
static void relayBidirectional(const char *myName, int a, int b) {
    uint8_t buf[TRAMA_SIZE];
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

        // Trama en direcció a->b (cap al destí)
        if (FD_ISSET(a, &readfds)) {
            if (relayReadFrame(a, buf) < 0) {
                break;
            }
            Frame f;
            deserializar_trama(buf, &f);
            if (!validateChecksum(&f)) {
                sendNack(a, myName, "CHECKSUM");  // NACK al hop anterior
                break;                            // descartem i tanquem el relay
            }
            if (writeAllBytes(b, buf, TRAMA_SIZE) < 0) {
                break;
            }
        }
        // Trama en direcció b->a (resposta cap a l'origen)
        if (FD_ISSET(b, &readfds)) {
            if (relayReadFrame(b, buf) < 0) {
                break;
            }
            Frame f;
            deserializar_trama(buf, &f);
            if (!validateChecksum(&f)) {
                sendNack(b, myName, "CHECKSUM");
                break;
            }
            if (writeAllBytes(a, buf, TRAMA_SIZE) < 0) {
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
            // Una ruta amb IP "*.*.*.*" (o port 0) vol dir que el regne existeix
            // però la seva ruta NO es coneix directament. A efectes d'enrutament
            // és com si no existís: cal caure al DEFAULT (com fa el costat origen).
            if (!maester->routes[i].ip ||
                strcasecmp(maester->routes[i].ip, "*.*.*.*") == 0 ||
                maester->routes[i].port <= 0) {
                continue;  // deixa que el segon bucle apliqui la ruta DEFAULT
            }
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

    // Connect amb timeout: posem el socket en no bloquejant i esperem amb select.
    // Així un node lent o caigut NO penja la consola (el dispatch a l'envoy és
    // síncron). Sense això, un connect() a un host inabastable bloqueja ~2 min.
    int flags = fcntl(fd_client, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd_client, F_SETFL, flags | O_NONBLOCK);
    }

    int rc = connect(fd_client, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (rc < 0 && errno == EINPROGRESS) {
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(fd_client, &wset);
        struct timeval tv;
        tv.tv_sec = CONNECT_TIMEOUT_SECONDS;
        tv.tv_usec = 0;

        rc = select(fd_client + 1, NULL, &wset, NULL, &tv);
        if (rc <= 0) {
            asprintf(&msg, RED "ERROR | Connection timeout to %s:%d\n" RESET, ip, targetPort);
            customWrite(1, msg);
            free(msg);
            close(fd_client);
            return -1;
        }
        // Connexió completada? Comprovem el codi d'error real del socket.
        int soErr = 0;
        socklen_t len = sizeof(soErr);
        if (getsockopt(fd_client, SOL_SOCKET, SO_ERROR, &soErr, &len) < 0 || soErr != 0) {
            asprintf(&msg, RED "ERROR | Connection refused to %s:%d\n" RESET, ip, targetPort);
            customWrite(1, msg);
            free(msg);
            close(fd_client);
            return -1;
        }
    } else if (rc < 0) {
        asprintf(&msg, RED "ERROR | Connection refused to %s:%d\n" RESET, ip, targetPort);
        customWrite(1, msg);
        free(msg);
        close(fd_client);
        return -1;
    }

    // Tornem a mode bloquejant per a sendFrame/receiveFrame.
    if (flags >= 0) {
        fcntl(fd_client, F_SETFL, flags);
    }

    // Timeout de lectura/escriptura: si el peer accepta però no contesta, les
    // lectures retornen error en comptes de bloquejar indefinidament.
    struct timeval io;
    io.tv_sec = IO_TIMEOUT_SECONDS;
    io.tv_usec = 0;
    setsockopt(fd_client, SOL_SOCKET, SO_RCVTIMEO, &io, sizeof(io));
    setsockopt(fd_client, SOL_SOCKET, SO_SNDTIMEO, &io, sizeof(io));

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

    // La primera trama ja s'ha reexpedit; ara reenviem trama a trama (validant
    // el checksum de cadascuna) la resta de la sessió (respostes, ACKs, dades...).
    relayBidirectional(maester->name, fromSocket, fd_hop);

    close(fd_hop);
    return 0;
}
