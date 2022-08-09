
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#define PROGRAM_NAME "init"

#include "systest.h"
#include "log.h"

void signalHandler(int signal) {
    USPACE_DEBUG("Signal handler");
    if (signal == SIGCHLD) {
        USPACE_DEBUG("Child signal. Waiting...");
        int status = 0;
        wait(&status);
        if (WIFEXITED(status)) {
            USPACE_DEBUG("Child exited with %i", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            USPACE_DEBUG("Child signaled with %i", WSTOPSIG(status));
        }
    }
}

void setupTty() {
    // Open file descriptors 0, 1 and 2
    for (int i = 0; i < 3; i++) {
        // 0 -> stdin, 1 -> stdout, 2 -> stderr
        int fd = open("/dev/tty0", i == 0 ? O_RDONLY : O_WRONLY);
        if (fd != i) {
            USPACE_ERROR(
                "Failed to open %s file: %s",
                i == 0   ? "stdin"
                : i == 1 ? "stdout"
                         : "stderr",
                strerror(errno)
            );
            exit(1);
        }
    }
}

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

void setupSystem() {
    signal(SIGCHLD, signalHandler);
    startProgram("/bin/hello");
}

noreturn void idleLoop() {
    // After setting up the system, for now we only wait for children
    for (;;) {
        pause();
        USPACE_DEBUG("Unpaused");
    }
}

int main(int argc, char* argv[], char* env[]) {
    setupTty();
    USPACE_SUCCESS("Started init process");
    if (runBasicSyscallTests()) {
        USPACE_SUCCESS("Finished basic syscall tests");
    } else {
        USPACE_WARNING("Failed basic syscall tests");
    }
    setupSystem();
    idleLoop();
    // We should not return
    USPACE_WARNING("Init process exited");
    return 1;
}

