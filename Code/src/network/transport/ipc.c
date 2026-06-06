#include "ipc.h"

static int writeAll(int fd, const void *buffer, size_t size) {
    const uint8_t *cursor = (const uint8_t *)buffer;
    size_t totalWritten = 0;

    while (totalWritten < size) {
        ssize_t written = write(fd, cursor + totalWritten, size - totalWritten);
        if (written <= 0) {
            return -1;
        }
        totalWritten += (size_t)written;
    }

    return 0;
}

static int readAll(int fd, void *buffer, size_t size) {
    uint8_t *cursor = (uint8_t *)buffer;
    size_t totalRead = 0;

    while (totalRead < size) {
        ssize_t bytesRead = read(fd, cursor + totalRead, size - totalRead);
        if (bytesRead <= 0) {
            return -1;
        }
        totalRead += (size_t)bytesRead;
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
