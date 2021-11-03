
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

void signalHandler(int signal) {
    fprintf(stderr, "[?] Signal handler\n");
    if (signal == SIGCHLD) {
        fprintf(stderr, "[?] Child exited. Waiting...\n");
        int status = 0;
        wait(&status);
        fprintf(stderr, "[?] Child exited with %i\n", WEXITSTATUS(status));
    }
}

void setupSystem() {
    signal(SIGCHLD, signalHandler);
    int pid = fork();
    if (pid == 0) {
        execl("/bin/hello", "/bin/hello", NULL);
        fprintf(stderr, "Failed to start `/bin/hello`: %s\n", strerror(errno));
        exit(1);
    } else {
        // Process started
        return;
    }
}

void idleLoop() {
    // After setting up the system, for now we only wait for children
    for (;;) {
        pause();
        fprintf(stderr, "[?] Unpaused\n");
    }
}

int main(int argc, char* argv[], char* env[]) {
    fprintf(stderr, "[+] Started init process\n");
    setupSystem();
    idleLoop();
    // We should not return
    fprintf(stderr, "[!] Init process exited\n");
    return 1;
}

