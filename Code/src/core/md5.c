#include "md5.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Calcula l'MD5 d'un buffer en MEMÒRIA (sense fitxers temporals): li passem les
// dades a md5sum per stdin (md5sum sense argument llegeix de stdin) i llegim el
// hash per stdout. Retorna 32 caràcters hex (cal free) o NULL.
char* md5_buffer(const uint8_t *data, size_t len) {
    int in[2], out[2];
    if (pipe(in) < 0) {
        return NULL;
    }
    if (pipe(out) < 0) {
        close(in[0]);
        close(in[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in[0]); close(in[1]);
        close(out[0]); close(out[1]);
        return NULL;
    }

    if (pid == 0) {
        // Fill: md5sum llegeix de stdin (in) i escriu a stdout (out)
        dup2(in[0], STDIN_FILENO);
        dup2(out[1], STDOUT_FILENO);
        close(in[0]); close(in[1]);
        close(out[0]); close(out[1]);
        execlp("md5sum", "md5sum", (char *)NULL);
        _exit(-1);
    }

    // Pare: tanquem els extrems que no fem servir
    close(in[0]);
    close(out[1]);

    // 1) Escrivim totes les dades a stdin de md5sum i tanquem (EOF -> calcula)
    size_t off = 0;
    while (off < len) {
        ssize_t w = write(in[1], data + off, len - off);
        if (w <= 0) {
            break;
        }
        off += (size_t)w;
    }
    close(in[1]);

    // 2) Llegim els 32 hex de la sortida
    char *result = malloc(33);
    if (!result) {
        close(out[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }
    int total = 0;
    ssize_t n;
    while (total < 32 && (n = read(out[0], result + total, 32 - total)) > 0) {
        total += (int)n;
    }
    result[total] = '\0';
    close(out[0]);
    waitpid(pid, NULL, 0);

    if (total < 32) {
        free(result);
        return NULL;
    }
    return result;
}
