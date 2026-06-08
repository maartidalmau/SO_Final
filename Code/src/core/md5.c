#include "md5.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void md5_init(MD5_CTX *ctx) {
    (void)ctx;
}

void md5_update(MD5_CTX *ctx, const uint8_t *data, uint64_t len) {
    (void)ctx;
    (void)data;
    (void)len;
}

void md5_final(MD5_CTX *ctx, uint8_t digest[16]) {
    (void)ctx;
    (void)digest;
}

char* md5_file(const char *filename) {
    if (!filename) return NULL;

    char command[512];
    char *result = malloc(33);
    if (!result) return NULL;

    FILE *fp = NULL;

#ifdef _WIN32
    snprintf(command, sizeof(command), "certutil -hashfile \"%s\" MD5 | findstr /v :", filename);
#else
    snprintf(command, sizeof(command), "md5sum \"%s\" | awk '{print $1}'", filename);
#endif

    fp = popen(command, "r");
    if (!fp) {
        free(result);
        return NULL;
    }

    if (fgets(result, 33, fp) == NULL) {
        pclose(fp);
        free(result);
        return NULL;
    }

    pclose(fp);

    char *newline = strchr(result, '\n');
    if (newline) *newline = '\0';

    char *carriage = strchr(result, '\r');
    if (carriage) *carriage = '\0';

    return result;
}

char* md5_buffer(const uint8_t *buffer, uint64_t len) {
    (void)buffer;
    (void)len;
    return NULL;
}
