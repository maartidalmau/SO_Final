#ifndef FRAMEHANDLER_H
#define FRAMEHANDLER_H

#define _GNU_SOURCE


#include "frame.h"
#include "router.h"
#include "dataStructures.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


/**
 * Dispatcher principal que procesa frames según su destino y tipo
 * @param maester Estructura del Maester
 * @param frame Frame a procesar
 * @param fromSocket Socket del que vino el frame
 */
void processFrame(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para PING_PONG (0x26)
 * Responde con el mismo frame (echo)
 */
void handlePingPong(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para MAESTER_DISCONNECT (0x27)
 * Procesa desconexión ordenada de un reino
 */
void handleDisconnect(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para NACK_ERROR (0x69)
 * Procesa errores reportados por otros nodos
 */
void handleNack(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para ALLIANCE_REQUEST (0x01)
 * Procesa solicitud de alianza
 */
void handleAllianceRequest(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para SIGIL_SEND (0x02)
 * Recibe datos del archivo sigil
 */
void handleSigilSend(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para ALLIANCE_RESPONSE (0x03)
 * Procesa respuesta a solicitud de alianza
 */
void handleAllianceResponse(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para PRODUCT_LIST_REQUEST (0x11)
 * Procesa solicitud de lista de productos
 */
void handleProductListRequest(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para ACK_FILE (0x31)
 * Procesa confirmación de archivo
 */
void handleAckFile(Maester *maester, Frame *frame, int fromSocket);

/**
 * Handler para ACK_MD5SUM (0x32)
 * Procesa confirmación de integridad MD5
 */
void handleAckMD5(Maester *maester, Frame *frame, int fromSocket);

#endif // FRAMEHANDLER_H
