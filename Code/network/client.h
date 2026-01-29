#ifndef CLIENT_H
#define CLIENT_H

#define _GNU_SOURCE

#include "frame.h"
#include "router.h"
#include "dataStructures.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

/**
 * Función auxiliar: Conecta a un reino y gestiona envoys
 * Bloquea si no hay envoys disponibles
 * Thread-safe: usa envoys_sem
 * @param maester Estructura del Maester
 * @param realmName Reino destino
 * @param raven_fd_out Socket de salida
 * @return 0 si éxito, -1 si error (libera envoy automáticamente en error)
 */
int connectToRealmWithEnvoy(Maester *maester, const char *realmName, int *raven_fd_out);

/**
 * Envía un PING a un reino para verificar conectividad
 * Gestiona envoys automáticamente
 * @param maester Estructura del Maester
 * @param realmName Reino destino
 * @return 0 si éxito, -1 si error
 */
int sendPing(Maester *maester, const char *realmName);

/**
 * Envía notificación de desconexión a todos los reinos conocidos
 * Se llama antes de apagar el servidor
 * @param maester Estructura del Maester
 */
void notifyDisconnect(Maester *maester);

/**
 * Envía una solicitud de alianza (PLEDGE)
 * @param maester Estructura del Maester
 * @param realmName Reino destino
 * @param sigilPath Ruta del archivo sigil.png
 * @return 0 si éxito, -1 si error
 */
int sendAllianceRequest(Maester *maester, const char *realmName, const char *sigilPath);

/**
 * Responde a una solicitud de alianza
 * @param maester Estructura del Maester
 * @param realmName Reino que hizo la petición
 * @param accept 1=ACCEPT, 0=REJECT
 * @return 0 si éxito, -1 si error
 */
int sendAllianceResponse(Maester *maester, const char *realmName, int accept);

/**
 * Solicita lista de productos de un reino
 * @param maester Estructura del Maester
 * @param realmName Reino destino
 * @return 0 si éxito, -1 si error
 */
int sendProductListRequest(Maester *maester, const char *realmName);

#endif // CLIENT_H
