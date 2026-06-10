#include "md5.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

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
            _exit(-1);
        }
        close(pipefd[1]);
        execlp("md5sum", "md5sum", filename, (char *)NULL);
        _exit(-1);  // només s'arriba aquí si execlp falla
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
    if (total < 32) {
        free(result);
        return NULL;
    }

    return result;
}
