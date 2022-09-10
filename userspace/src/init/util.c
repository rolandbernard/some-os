
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PROGRAM_NAME "init"
#include "log.h"

void startProgram(const char* name, bool give_terminal) {
    int pid = fork();
    if (pid == 0) {
        setsid();
        if (give_terminal) {
            tcsetpgrp(0, getpgrp());
        }
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
    bool have_terminal = (getpgrp() == tcgetpgrp(0));
    int pid = fork();
    if (pid == 0) {
        setsid();
        if (have_terminal) {
            tcsetpgrp(0, getpgrp());
        }
        execl(name, name, NULL);
        USPACE_ERROR("Failed to start `%s`: %s", name, strerror(errno));
        exit(1);
    } else {
        USPACE_DEBUG("Forked");
        int status;
        while (waitpid(pid, &status, 0) != pid);
        if (have_terminal) {
            tcsetpgrp(0, getpgrp());
        }
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
}

