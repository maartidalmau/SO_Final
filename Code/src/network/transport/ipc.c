#include "ipc.h"
#include <sys/select.h>
#include <time.h>

#define IPC_TIMEOUT_SECONDS 30

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

static int readAllWithTimeout(int fd, void *buffer, size_t size, int timeoutSeconds) {
    uint8_t *cursor = (uint8_t *)buffer;
    size_t totalRead = 0;
    time_t startTime = time(NULL);

    while (totalRead < size) {
        fd_set readfds;
        struct timeval tv;

        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        tv.tv_sec = timeoutSeconds;
        tv.tv_usec = 0;

        int selectResult = select(fd + 1, &readfds, NULL, NULL, &tv);

        if (selectResult < 0) {
            return -1;
        }

        if (selectResult == 0) {
            return -1;
        }

        time_t now = time(NULL);
        if ((now - startTime) > timeoutSeconds) {
            return -1;
        }

        ssize_t bytesRead = read(fd, cursor + totalRead, size - totalRead);
        if (bytesRead <= 0) {
            return -1;
        }
        totalRead += (size_t)bytesRead;
    }

    return 0;
}

static int readAll(int fd, void *buffer, size_t size) {
    return readAllWithTimeout(fd, buffer, size, IPC_TIMEOUT_SECONDS);
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
