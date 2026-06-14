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
        unsigned long dataLen = strlen(data);
        if (dataLen > DATA_MAX_SIZE) {
            dataLen = DATA_MAX_SIZE;
        }
        memcpy(frame->data, data, dataLen);
        frame->data_length = dataLen;
    } else {
        frame->data_length = 0;
    }

    // Calculate the checksum
    frame->checksum = calcChecksum(frame);
}

void createBinaryFrame(Frame *frame, uint8_t type, const char *origin, const char *destination, const void *data, uint16_t length) {
    if (!frame) return;

    // Initialize the frame with zeros (padding inclòs)
    memset(frame, 0, sizeof(Frame));

    frame->type = type;

    if (origin) {
        strncpy(frame->ip_origin, origin, IP_SIZE - 1);
        frame->ip_origin[IP_SIZE - 1] = '\0';
    }

    if (destination) {
        strncpy(frame->ip_destination, destination, IP_SIZE - 1);
        frame->ip_destination[IP_SIZE - 1] = '\0';
    }

    // Còpia segura de dades BINÀRIES: longitud explícita, sense strlen.
    if (length > DATA_MAX_SIZE) {
        length = DATA_MAX_SIZE;
    }
    if (data && length > 0) {
        memcpy(frame->data, data, length);
    }
    frame->data_length = length;  // sempre en host order (com createFrame)

    frame->checksum = calcChecksum(frame);
}

int sendNack(int fd, const char *realmName, const char *errorCode) {
    if (fd < 0 || !realmName) {
        return -1;
    }
    
    // Log the error
    char *msg;
    asprintf(&msg, RED "Error [%s]\n" RESET, errorCode);
    customWrite(1, msg);
    free(msg);
    
    // Create and send NACK frame (TYPE 0x69, ORIGIN/DESTINATION buits)
    Frame nackFrame;
    createFrame(&nackFrame, NACK_ERROR, "", "", realmName);
    
    return sendFrame(fd, &nackFrame);
}

int validateChecksum(const Frame *frame) {
    if (!frame) return 0;

    // Calculate the expected checksum
    uint16_t expectedChecksum = calcChecksum(frame);

    // Compare with the received checksum
    return (expectedChecksum == frame->checksum);
}

int sendFrame(int fd_client, Frame *frame) {
    if (fd_client < 0 || !frame) {
        return -1;
    }

    // Buffer para serializar la trama (320 bytes)
    uint8_t buffer[TRAMA_SIZE];

    // Serializar la trama
    serializar_trama(frame, buffer);

    // Escriptura ÚNICA de la trama sencera (320B), tal com indica l'enunciat
    // (Annex II): "escriure amb un sol write de 320B".
    long sent = write(fd_client, buffer, TRAMA_SIZE);
    if (sent != TRAMA_SIZE) {
        // Error, connexió tancada o escriptura parcial: trama no enviada
        return -1;
    }

    return 0;
}

int receiveFrame(int fd_client, Frame *frame) {
    if (fd_client < 0 || !frame) {
        return -1;
    }

    // Buffer para recibir la trama (320 bytes)
    uint8_t buffer[TRAMA_SIZE];

    // Lectura ÚNICA de la trama sencera (320B), tal com indica l'enunciat
    // (Annex II): "llegir amb un sol read ... de 320B".
    long received = read(fd_client, buffer, TRAMA_SIZE);
    if (received != TRAMA_SIZE) {
        // Error, connexió tancada o lectura parcial: trama no vàlida
        return -1;
    }

    // Deserializar la trama
    deserializar_trama(buffer, frame);

    return 0;
}
