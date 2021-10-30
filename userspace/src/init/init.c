
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void setupSystem() {
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
        int status = 0;
        wait(&status);
    }
}

int main(int argc, char* argv[], char* env[]) {
    setupSystem();
    idleLoop();
    // We should not return
    fprintf(stderr, "Init process exited");
    return 1;
}

