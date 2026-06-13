#ifndef MD5_H
#define MD5_H

#include <stdint.h>
#include <stdio.h>

char* md5_buffer(const uint8_t *data, unsigned long len);

#endif // MD5_H
