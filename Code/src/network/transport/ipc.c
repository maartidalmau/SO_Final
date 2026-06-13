#include "ipc.h"

static int writeAll(int fd, const void *buffer, unsigned long size) {
    const uint8_t *cursor = (const uint8_t *)buffer;
    unsigned long totalWritten = 0;

    while (totalWritten < size) {
        long written = write(fd, cursor + totalWritten, size - totalWritten);
        if (written <= 0) {
            return -1;
        }
        totalWritten += (unsigned long)written;
    }

    return 0;
}

// Lectura bloquejant completa: el canal IPC Maester<->Envoy és un pipe en el qual
// l'envoy espera  una missió (potser molt de temps). NO posem timeout aquí:
// si poséssim, l'envoy "moriria" tot sol després d'uns segons sense feina i un
// write() posterior del Maester rebria SIGPIPE. read() retorna 0 (EOF) si l'altre
// extrem tanca el pipe (p. ex. el Maester acaba), cosa que sí trenca el bucle net.
static int readAll(int fd, void *buffer, unsigned long size) {
    uint8_t *cursor = (uint8_t *)buffer;
    unsigned long totalRead = 0;

    while (totalRead < size) {
        unsigned long bytesRead = read(fd, cursor + totalRead, size - totalRead);
        if (bytesRead <= 0) {
            return -1;
        }
        totalRead += (unsigned long)bytesRead;
    }

    return 0;
}

int sendIpcRequest(int fd, const IpcRequest *request) {
    if (!request) {
        return -1;
    }
    return writeAll(fd, request, sizeof(IpcRequest));
}

int receiveIpcRequest(int fd, IpcRequest *request) {
    if (!request) {
        return -1;
    }
    return readAll(fd, request, sizeof(IpcRequest));
}

int sendIpcResponse(int fd, const IpcResponse *response) {
    if (!response) {
        return -1;
    }
    return writeAll(fd, response, sizeof(IpcResponse));
}

int receiveIpcResponse(int fd, IpcResponse *response) {
    if (!response) {
        return -1;
    }
    return readAll(fd, response, sizeof(IpcResponse));
}
