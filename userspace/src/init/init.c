
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>

int main(int argc, char* argv[], char* env[]) {
    int pipes[2];
    pipe(pipes);
    int pid = fork();
    if (pid == 0) {
        char* data = malloc(512);
        data[0] = 'N';
        data[1] = 'o';
        data[2] = 0;
        read(pipes[0], data, 512);
        printf("CHILD %s\n", data);
        pid = getpid();
        printf("PID  = %i\n", pid);
        pid = getppid();
        printf("PPID = %i\n", pid);
    } else {
        printf("kill: %i\n", kill(pid, SIGKILL));
        usleep(1000000);
        write(pipes[1], "Test", 4);
        close(pipes[0]);
        close(pipes[1]);
        int status = 0;
        int pid = wait(&status);
        char* data = malloc(100);
        printf("PARENT of %i state %i\n", pid, status);
        struct tms tims;
        printf("time: %lu\n", times(&tims));
        printf("ut: %lu st: %lu uct: %lu sct: %lu\n", tims.tms_utime, tims.tms_stime, tims.tms_cutime, tims.tms_cstime);
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

