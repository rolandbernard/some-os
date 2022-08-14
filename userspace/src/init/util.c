
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define PROGRAM_NAME "init"
#include "log.h"

void startProgram(const char* name) {
    int pid = fork();
    if (pid == 0) {
        execl(name, name, NULL);
        USPACE_ERROR("Failed to start `%s`: %s", name, strerror(errno));
        exit(1);
    } else {
        USPACE_DEBUG("Forked");
        // Process started
        return;
    }
}

int runProgram(const char* name) {
    int pid = fork();
    if (pid == 0) {
        execl(name, name, NULL);
        USPACE_ERROR("Failed to start `%s`: %s", name, strerror(errno));
        exit(1);
    } else {
        USPACE_DEBUG("Forked");
        int status;
        while (waitpid(pid, &status, 0) != pid);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
}

