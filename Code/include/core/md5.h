#ifndef MD5_H
#define MD5_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint32_t A, B, C, D;
    uint64_t count;
    uint8_t buffer[64];
} MD5_CTX;

void md5_init(MD5_CTX *ctx);
void md5_update(MD5_CTX *ctx, const uint8_t *data, uint64_t len);
void md5_final(MD5_CTX *ctx, uint8_t digest[16]);

char* md5_file(const char *filename);
char* md5_buffer(const uint8_t *buffer, uint64_t len);

#endif // MD5_H
