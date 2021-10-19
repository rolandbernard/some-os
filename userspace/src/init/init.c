
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>

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
        char* data = malloc(100);
        printf("PARENT of %i state %i\n", pid, status);
        chdir("/dev");
        DIR* dir = opendir(".");
        printf("Devices in %s: %p\n", getcwd(data, 100), dir);
        struct dirent* ent;
        do {
            ent = readdir(dir);
            if (ent != NULL) {
                printf(" %s\n", ent->d_name);
            }
        } while (ent != NULL);
        closedir(dir);
        chdir("/");
        for (int i = 0;; i++) {
            printf("Sleeping... %i\n", i);
            usleep(1000000UL);
        }
    }
    return 1;
}

