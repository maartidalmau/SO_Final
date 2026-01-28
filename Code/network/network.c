#include "network.h"

uint16_t calcChecksum(const Frame *frame) {
    uint32_t suma = 0;
    
    // Sumar el byte TYPE
    suma += frame->type;
    
    // Sumar los bytes de ORIGIN (20 bytes)
    for (int i = 0; i < IP_SIZE; i++) {
        suma += (uint8_t)frame->ip_origin[i];
    }
    
    // Sumar los bytes de DESTINATION (20 bytes)
    for (int i = 0; i < IP_SIZE; i++) {
        suma += (uint8_t)frame->ip_destination[i];
    }
    
    // Sumar DATA_LENGTH (2 bytes en network byte order)
    uint16_t data_length_net = htons(frame->data_lenght);
    uint8_t *data_length_bytes = (uint8_t *)&data_length_net;
    suma += data_length_bytes[0] + data_length_bytes[1];
    
    // Sumar los bytes de DATA (hasta DATA_MAX_SIZE bytes)
    for (int i = 0; i < DATA_MAX_SIZE; i++) {
        suma += (uint8_t)frame->data[i];
    }
    
    // Aplicar módulo 32768 
    return (uint16_t)(suma % 32768);
}

// Función para serializar una trama a un buffer de 320 bytes
void serializar_trama(const Frame *frame, uint8_t *buffer) {
    int offset = 0;
    
    // Inicializar buffer con padding (0x00)
    memset(buffer, 0, TRAMA_SIZE);
    
    // TYPE (1 byte)
    buffer[offset] = frame->type;
    offset += 1;
    
    // ORIGIN (20 bytes)
    memcpy(buffer + offset, frame->ip_origin, IP_SIZE);
    offset += IP_SIZE;
    
    // DESTINATION (20 bytes)
    memcpy(buffer + offset, frame->ip_destination, IP_SIZE);
    offset += IP_SIZE;
    
    // DATA_LENGTH (2 bytes en network byte order)
    uint16_t data_length_net = htons(frame->data_lenght);
    memcpy(buffer + offset, &data_length_net, sizeof(uint16_t));
    offset += sizeof(uint16_t);
    
    // DATA (DATA_MAX_SIZE bytes)
    memcpy(buffer + offset, frame->data, DATA_MAX_SIZE);
    offset += DATA_MAX_SIZE;
    
    // CHECKSUM (2 bytes en network byte order)
    uint16_t checksum_net = htons(frame->checksum);
    memcpy(buffer + offset, &checksum_net, sizeof(uint16_t));
}

// Función para deserializar un buffer de 320 bytes a una estructura Frame
void deserializar_trama(const uint8_t *buffer, Frame *frame) {
    int offset = 0;
    
    // TYPE (1 byte)
    frame->type = buffer[offset];
    offset += 1;
    
    // ORIGIN (20 bytes)
    memcpy(frame->ip_origin, buffer + offset, IP_SIZE);
    offset += IP_SIZE;
    
    // DESTINATION (20 bytes)
    memcpy(frame->ip_destination, buffer + offset, IP_SIZE);
    offset += IP_SIZE;
    
    // DATA_LENGTH (2 bytes, convertir de network byte order)
    uint16_t data_length_net;
    memcpy(&data_length_net, buffer + offset, sizeof(uint16_t));
    frame->data_lenght = ntohs(data_length_net);
    offset += sizeof(uint16_t);
    
    // DATA (DATA_MAX_SIZE bytes)
    memcpy(frame->data, buffer + offset, DATA_MAX_SIZE);
    offset += DATA_MAX_SIZE;
    
    // CHECKSUM (2 bytes, convertir de network byte order)
    uint16_t checksum_net;
    memcpy(&checksum_net, buffer + offset, sizeof(uint16_t));
    frame->checksum = ntohs(checksum_net);
}