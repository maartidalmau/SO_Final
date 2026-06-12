#include "utils.h"

pthread_mutex_t stdoutMutex = PTHREAD_MUTEX_INITIALIZER;

void safeFree(void** ptr) {
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

void customWrite(int fdesc, char* string) {
    pthread_mutex_lock(&stdoutMutex);
    write(fdesc, string, strlen(string));
    pthread_mutex_unlock(&stdoutMutex);
}

void trimNewline(char *str) {
    if (!str) return;
    int len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
}

int customRead(int fdesc, char **string, char delim) {
    char buffer;
    int bytesRead;
    int len = 0;
    char *tmp;

    if (!string) {
        return -1;
    }
    *string = NULL;

    while (1) {
        bytesRead = read(fdesc, &buffer, 1);
        if (bytesRead < 0)
        {
            safeFree((void **)string);
            return -1;
        }

        if (bytesRead == 0)
            break;

        if (buffer == delim)
            break;

        tmp = realloc(*string, len + 2);
        if (!tmp)
        {
            safeFree((void **)string);
            return -1;
        }

        *string = tmp;
        (*string)[len++] = buffer;
        (*string)[len] = '\0';
    }

    if (len == 0 && bytesRead == 0) {
        return 0;
    }

    if (*string) {
        trimNewline(*string);
    }
    return 1;
}

void removeChar(char *str, char c) {
    if (str == NULL) {
        return;
    }
    
    int i = 0, j = 0;
    while (str[i]) {
        if (str[i] != c) {
            str[j++] = str[i];
        }
        i++;
    }
    str[j] = '\0';
}

int parseCommand(char *command, char *tokens[]) {
    int count = 0;

    if (command == NULL) {
        return 0;
    }

    char *token = strtok(command, " ");
    while (token && count < MAX_TOKENS) {
        tokens[count++] = token;
        token = strtok(NULL, " ");
    }
    return count;
}