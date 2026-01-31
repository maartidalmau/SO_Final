#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#define TRAMA_SIZE 320
#define DATA_MAX_SIZE (TRAMA_SIZE - 45)
#define IP_SIZE 20


typedef struct {
    uint8_t type; //para valores alfanumericos mejor uint8 que char 
    char ip_origin[IP_SIZE];
    char ip_destination[IP_SIZE];
    uint16_t data_length;
    char data[DATA_MAX_SIZE];
    uint16_t checksum;
} __attribute__((packed)) Frame;

uint16_t calcChecksum(const Frame *frame);
void serializar_trama(const Frame *frame, uint8_t *buffer);
void deserializar_trama(const uint8_t *buffer, Frame *frame);

#endif