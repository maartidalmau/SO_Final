#include "frame.h"


void createFrame(Frame *frame, uint8_t type, const char *origin, const char *destination, const char *data) {
    if (!frame) return;

    // Initialize the frame with zeros
    memset(frame, 0, sizeof(Frame));

    // Set the type of the frame
    frame->type = type;

    // Copy the origin (reino de origen)
    if (origin) {
        strncpy(frame->ip_origin, origin, IP_SIZE - 1);
        frame->ip_origin[IP_SIZE - 1] = '\0'; // Ensure null termination
    }

    // Copy the destination (reino de destino)
    if (destination) {
        strncpy(frame->ip_destination, destination, IP_SIZE - 1);
        frame->ip_destination[IP_SIZE - 1] = '\0'; // Ensure null termination
    }

    // Copy the data if provided
    if (data) {
        size_t dataLen = strlen(data);
        if (dataLen > DATA_MAX_SIZE) {
            dataLen = DATA_MAX_SIZE;
        }
        memcpy(frame->data, data, dataLen);
        frame->data_lenght = dataLen;
    } else {
        frame->data_lenght = 0;
    }

    // Calculate the checksum
    frame->checksum = calcChecksum(frame);
}

void createNackFrame(Frame *frame, const char *realmName) {
    if (!frame) return;

    // Initialize the frame with zeros
    memset(frame, 0, sizeof(Frame));

    // TYPE: 0x69
    frame->type = NACK_ERROR;

    // ORIGIN: Empty (already zeroed)
    // DESTINATION: Empty (already zeroed)

    // DATA: <RealmName> that detected the error
    if (realmName) {
        size_t dataLen = strlen(realmName);
        if (dataLen > DATA_MAX_SIZE) {
            dataLen = DATA_MAX_SIZE;
        }
        memcpy(frame->data, realmName, dataLen);
        frame->data_lenght = dataLen;
    } else {
        frame->data_lenght = 0;
    }

    // Calculate the checksum
    frame->checksum = calcChecksum(frame);
}

int sendNack(int fd, const char *realmName, const char *errorCode) {
    if (fd < 0 || !realmName) {
        return -1;
    }
    
    // Log the error
    char *msg;
    asprintf(&msg, RED "Els corbs s'han perdut - Error [%s]\n" RESET, errorCode);
    customWrite(1, msg);
    free(msg);
    
    // Create and send NACK frame
    Frame nackFrame;
    createNackFrame(&nackFrame, realmName);
    
    return sendFrame(fd, &nackFrame);
}

int validateChecksum(const Frame *frame) {
    if (!frame) return 0;

    // Calculate the expected checksum
    uint16_t expectedChecksum = calcChecksum(frame);

    // Compare with the received checksum
    return (expectedChecksum == frame->checksum);
}

int sendFrame(int raven_fd_client, Frame *frame) {
    if (raven_fd_client < 0 || !frame) {
        return -1;
    }

    // Buffer para serializar la trama (320 bytes)
    uint8_t buffer[TRAMA_SIZE];
    
    // Serializar la trama
    serializar_trama(frame, buffer);
    
    // Enviar los 320 bytes por el socket
    ssize_t totalSent = 0;
    ssize_t bytesLeft = TRAMA_SIZE;
    
    while (totalSent < TRAMA_SIZE) {
        ssize_t sent = write(raven_fd_client, buffer + totalSent, bytesLeft);
        
        if (sent < 0) {
            // Error al enviar
            return -1;
        }
        
        if (sent == 0) {
            // Conexión cerrada
            return -1;
        }
        
        totalSent += sent;
        bytesLeft -= sent;
    }
    
    return 0;
}

int receiveFrame(int raven_fd_client, Frame *frame) {
    if (raven_fd_client < 0 || !frame) {
        return -1;
    }

    // Buffer para recibir la trama (320 bytes)
    uint8_t buffer[TRAMA_SIZE];
    
    // Recibir los 320 bytes del socket
    ssize_t totalReceived = 0;
    ssize_t bytesLeft = TRAMA_SIZE;
    
    while (totalReceived < TRAMA_SIZE) {
        ssize_t received = read(raven_fd_client, buffer + totalReceived, bytesLeft);
        
        if (received < 0) {
            // Error al recibir
            return -1;
        }
        
        if (received == 0) {
            // Conexión cerrada por el otro lado
            return -1;
        }
        
        totalReceived += received;
        bytesLeft -= received;
    }
    
    // Deserializar la trama
    deserializar_trama(buffer, frame);
    
    return 0;
}
