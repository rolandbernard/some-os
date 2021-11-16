
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

void signalHandler(int signal) {
    fprintf(stderr, "[?] Signal handler\n");
    if (signal == SIGCHLD) {
        fprintf(stderr, "[?] Child exited. Waiting...\n");
        int status = 0;
        wait(&status);
        fprintf(stderr, "[?] Child exited with %i\n", WEXITSTATUS(status));
    }
}

void setupTty() {
    // Open file descriptors 0, 1 and 2
    for (int i = 0; i < 3; i++) {
        // 0 -> stdin, 1 -> stdout, 2 -> stderr
        int fd = open("/dev/tty0", i == 0 ? O_RDONLY : O_WRONLY);
        if (fd != i) {
            fprintf(
                stderr, "[!] Failed to open %s file: %s",
                i == 0   ? "stdin"
                : i == 1 ? "stdout"
                         : "stderr",
                strerror(errno)
            );
            exit(1);
        }
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
    setupTty();
    fprintf(stderr, "[+] Started init process\n");
    setupSystem();
    sleep(1); // This is here just now for testing
    int fd = open("/fifo", O_RDWR);
    const char* msg = "Test message";
    write(fd, msg, strlen(msg));
    idleLoop();
    // We should not return
    fprintf(stderr, "[!] Init process exited\n");
    return 1;
}

