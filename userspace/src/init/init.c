
#include <stdbool.h>

#include "libstart/syscall.h"

void itoa(char* dest, long num) {
    size_t idx = 0;
    bool neg = false;;
    if (num < 0) {
        num = -num;
        neg = true;
    }
    do {
        dest[idx++] = '0' + (num % 10);
        num /= 10;
    } while (num != 0);
    if (neg) {
        dest[idx++] = '-';
    }
    for (size_t i = 0; 2 * i < idx; i++) {
        char tmp = dest[i];
        dest[i] = dest[idx - i - 1];
        dest[idx - i - 1] = tmp;
    }
    dest[idx] = 0;
}

volatile bool cont = false;

void child() {
    void* data = syscall_sbrk(128);
    int status;
    int pid = syscall_wait(0, &status);
    syscall_print("PARENT of ");
    itoa((char*)data, pid);
    syscall_print((char*)data);
    syscall_print(" state ");
    itoa((char*)data, status);
    syscall_print((char*)data);
    syscall_print("\n");
    cont = true;
}

void sigret() {
    syscall_sigreturn();
}

int main(int argc, char* argv[], char* env[]) {
    void* data = syscall_sbrk(128);
    DirectoryEntry* entry = (DirectoryEntry*)data;
    SigAction action = {
        .handler = child,
        .restorer = sigret,
    };
    syscall_sigaction(SIGCHLD, &action, NULL);
    int pid = syscall_fork();
    if (pid == 0) {
        syscall_print("CHILD\n");
        syscall_print("PID  = ");
        pid = syscall_getpid();
        itoa((char*)data, pid);
        syscall_print((char*)data);
        syscall_print("\n");
        syscall_print("PPID = ");
        pid = syscall_getppid();
        itoa((char*)data, pid);
        syscall_print((char*)data);
        syscall_print("\n");
    } else {
        while (!cont) {};
        int fd = syscall_open("/test", FILE_OPEN_DIRECTORY, 0);
        while ((int)syscall_readdir(fd, entry, 512) > 0) {
            syscall_print("Entry: ");
            syscall_print(entry->name);
            syscall_print("\n");
        }
        syscall_print("Hello world!\n");
        syscall_close(fd);
        for (int i = 0;; i++) {
            syscall_print("Sleeping... ");
            itoa((char*)data, i);
            syscall_print((char*)data);
            syscall_print("\n");
            syscall_sleep(1000000000UL);
        }
    }
    return 1;
}

