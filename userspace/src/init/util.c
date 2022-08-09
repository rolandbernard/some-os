
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

void startProgram(const char* name) {
    int pid = fork();
    if (pid == 0) {
        USPACE_DEBUG("Forked");
        execl(name, name, NULL);
        USPACE_ERROR("Failed to start `%s`: %s", name, strerror(errno));
        exit(1);
    } else {
        // Process started
        return;
    }
}

