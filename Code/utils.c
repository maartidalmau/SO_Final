#include "utils.h"

void* safeMalloc(int size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        //Print ERROR
        return NULL;
    }
    return ptr;
}

void safeFree(void** ptr) {
    if (ptr != NULL && *ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}

int isAllocated(void* ptr) {
    return ptr != NULL;
}

void customWrite(int fdesc, char* string) {
    write(fdesc, string, strlen(string));
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


char* intToStr(int num) {
    char* str = malloc(12);

    if (!str) {
        return NULL;
    }

    int i = 0;
    if (num == 0) {
        str[i++] = '0';
    } else {
        if (num < 0) {
            str[i++] = '-';
            num = -num;
        }

        int start = i;

        while (num > 0) {
            str[i++] = (num % 10) + '0';
            num /= 10;
        }

        for (int j = 0; j < (i - start) / 2; j++) {
            char temp = str[start + j];
            str[start + j] = str[i - 1 - j];
            str[i - 1 - j] = temp;
        }
    }

    str[i] = '\0';
    return str;
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