#include "md5.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Aquestes funcions formen part d'una API d'MD5 "manual" que NO usem: el
// enunciat obliga a calcular l'MD5 executant la comanda md5sum del sistema.
// Es mantenen com a stubs buits per compatibilitat de la capçalera.
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

// Calcula l'MD5 d'un fitxer executant la comanda md5sum del sistema operatiu.
// Usem fork + execlp + pipe (NO popen/system, que estan prohibits) i llegim
// la sortida amb read(). Retorna una cadena de 32 caràcters (cal free) o NULL.
char* md5_file(const char *filename) {
    if (!filename) return NULL;

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        // Fill: redirigim stdout cap al pipe i executem md5sum <filename>
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(127);
        }
        close(pipefd[1]);
        execlp("md5sum", "md5sum", filename, (char *)NULL);
        _exit(127);  // només s'arriba aquí si execlp falla
    }

    // Pare: llegim els 32 caràcters hexadecimals de l'MD5 des del pipe
    close(pipefd[1]);

    char *result = malloc(33);
    if (!result) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return NULL;
    }

    int total = 0;
    ssize_t n;
    while (total < 32 && (n = read(pipefd[0], result + total, 32 - total)) > 0) {
        total += (int)n;
    }
    result[total] = '\0';

    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    // md5sum ha de retornar 32 hex; si no, considerem error
    if (total < 32 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(result);
        return NULL;
    }

    return result;
}

char* md5_buffer(const uint8_t *buffer, uint64_t len) {
    (void)buffer;
    (void)len;
    return NULL;
}
