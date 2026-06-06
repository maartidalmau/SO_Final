#ifndef IPC_H
#define IPC_H

#include "dataStructures.h"

#include <stdint.h>

#define IPC_REALM_SIZE 64
#define IPC_IP_SIZE 32
#define IPC_PATH_SIZE 256
#define IPC_PAYLOAD_SIZE 512

typedef enum {
    IPC_NONE = 0,
    IPC_PLEDGE_REQUEST = 1,
    IPC_PLEDGE_RESPOND = 2,
    IPC_LIST_PRODUCTS_REMOTE = 3,
    IPC_SHUTDOWN = 255
} IpcRequestType;

typedef enum {
    IPC_STATUS_OK = 0,
    IPC_STATUS_ERROR = 1,
    IPC_STATUS_TIMEOUT = 2,
    IPC_STATUS_REMOTE_ERROR = 3
} IpcStatus;

typedef struct {
    uint32_t type;
    uint32_t request_id;
    uint32_t aux_value;
    char source_realm[IPC_REALM_SIZE];
    char source_ip[IPC_IP_SIZE];
    uint32_t source_port;
    char target_realm[IPC_REALM_SIZE];
    char target_ip[IPC_IP_SIZE];
    uint32_t target_port;
    char path[IPC_PATH_SIZE];
} IpcRequest;

typedef struct {
    uint32_t request_id;
    uint32_t status;
    uint32_t frame_type;
    int32_t result_code;
    char realm[IPC_REALM_SIZE];
    char payload[IPC_PAYLOAD_SIZE];
} IpcResponse;

int sendIpcRequest(int fd, const IpcRequest *request);
int receiveIpcRequest(int fd, IpcRequest *request);

int sendIpcResponse(int fd, const IpcResponse *response);
int receiveIpcResponse(int fd, IpcResponse *response);

#endif
