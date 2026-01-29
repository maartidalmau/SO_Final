#ifndef ROUTER_H
#define ROUTER_H

#define _GNU_SOURCE

#include "network.h"
#include "frame.h"
#include "dataStructures.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>




/**
 * Verifica si el destino de la trama es este Maester
 * @param maester Estructura del Maester
 * @param destination Nombre del reino destino
 * @return 1 si es el destinatario, 0 si no
 */
int isDestination(Maester *maester, const char *destination);

/**
 * Busca una ruta específica en la tabla de rutas (NO incluye DEFAULT)
 * Thread-safe: usa routes_mutex
 * @param maester Estructura del Maester
 * @param realmName Nombre del reino a buscar
 * @return Puntero a Route si existe, NULL si no
 */
Route* findRoute(Maester *maester, const char *realmName);

/**
 * Obtiene la ruta DEFAULT del Maester
 * Thread-safe: usa routes_mutex
 * @param maester Estructura del Maester
 * @return Puntero a Route si existe DEFAULT, NULL si no
 */
Route* getDefaultRoute(Maester *maester);

/**
 * Conecta a un reino específico usando IP y puerto
 * Aplica la regla de puerto +2 y muestra mensajes de diagnóstico
 * @param ip Dirección IP del reino
 * @param port Puerto base del reino
 * @param raven_fd_out Variable donde se almacenará el descriptor del socket
 * @return 0 si éxito, -1 si error
 */
int connectToRealmByRoute(const char *ip, int port, int *raven_fd_out);

/**
 * Conecta a un reino usando una estructura Route
 * @param route Ruta del reino
 * @param raven_fd_out Variable donde se almacenará el descriptor del socket
 * @return 0 si éxito, -1 si error
 */
int connectToRealm(Route *route, int *raven_fd_out);

/**
 * Reenvía una trama al siguiente hop según la tabla de rutas
 * Thread-safe: usa routes_mutex para buscar rutas
 * @param maester Estructura del Maester
 * @param frame Trama a reenviar (NO se modifica)
 * @param fromSocket Socket del que vino la trama (para responder ACK/NACK)
 * @return 0 si éxito, -1 si error
 */
int forwardFrame(Maester *maester, Frame *frame, int fromSocket);

#endif // ROUTER_H
