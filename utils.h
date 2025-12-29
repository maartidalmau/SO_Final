#ifndef SOF1_UTILS_H
#define SOF1_UTILS_H

#define _GNU_SOURCE   // necesario para asprintf
#include <stdio.h>    // asprintf, printf, etc.
#include <stdlib.h>   // malloc, free, exit
#include <string.h>   // strdup, strcpy, strlen
#include <strings.h>  // strcasecmp
#include <unistd.h>   // read, write, close

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define RESET "\x1B[0m"

#define MAX_TOKENS 8

void* safeMalloc(int size);

void safeFree(void** ptr);

int isAllocated(void* ptr);

void customWrite(int fdesc, char* string);

void concatAndPrint(int fdesc, char* strings[], int n);

int customRead(int fdesc, char** string, char delim);

char* intToStr(int num);

int parseCommand(char *command, char *tokens[]);

void removeChar(char *str, char c);

#endif // SOF1_UTILS_H
