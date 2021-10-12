
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>

void __trunctfdf2() {
    // Just here to avoid linker error
}

int main(int argc, char* argv[], char* env[]) {
    int pid = fork();
    if (pid == 0) {
        printf("CHILD\n");
        pid = getpid();
        printf("PID  = %i\n", pid);
        pid = getppid();
        printf("PPID = %i\n", pid);
    } else {
        int status = 0;
        int pid = wait(&status);
        printf("PARENT of %i state %i\n", pid, status);
        for (int i = 0;; i++) {
            printf("Sleeping... %i\n", i);
            usleep(1000000UL);
        }
    }
    return 1;
}

