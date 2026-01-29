#ifndef FRAME_H
#define FRAME_H

#include "network.h"
#include "dataStructures.h"

#include <unistd.h>
#include <sys/socket.h>

// Frame types
#define ALLIANCE_REQUEST       0x01  // Header + File info
#define SIGIL_SEND             0x02  // File data (Sigil)
#define ALLIANCE_RESPONSE      0x03  // Accept or Reject

#define PRODUCT_LIST_REQUEST   0x11  // Request inventory
#define PRODUCT_LIST_HEADER    0x12  // Inventory file info
#define PRODUCT_LIST_DATA      0x13  // Inventory file data
#define ORDER_REQUEST_HEADER   0x14  // Shopping list file info
#define ORDER_REQUEST_DATA     0x15  // Shopping list file data
#define ORDER_RESPONSE         0x16  // Status (OK/REJECT)

#define ERR_UNKNOWN_REALM      0x21  // Routing error
#define ERR_UNAUTHORIZED       0x25  // No alliance/auth
#define PING_PONG              0x26  // Liveness diagnostic
#define MAESTER_DISCONNECT     0x27  // Graceful shutdown

#define ACK_FILE               0x31  // Connection status (OK/KO)
#define ACK_MD5SUM             0x32  // Integrity check (CHECK_OK/KO)
#define NACK_ERROR             0x69  // Format or Checksum error

// Pledge response values
#define PLEDGE_ACCEPT 1
#define PLEDGE_REJECT 0


void createFrame(Frame *frame, uint8_t type, const char *origin, const char *destination, const char *data);

int validateChecksum(const Frame *frame);

/**
 * Envía una trama serializada a través de un socket
 * @param raven_fd_client Descriptor del socket
 * @param frame Trama a enviar
 * @return 0 si éxito, -1 si error
 */
int sendFrame(int raven_fd_client, Frame *frame);

/**
 * Recibe una trama desde un socket y la deserializa
 * @param raven_fd_client Descriptor del socket
 * @param frame Estructura donde se almacenará la trama recibida
 * @return 0 si éxito, -1 si error
 */
int receiveFrame(int raven_fd_client, Frame *frame);

#endif // FRAME_H
