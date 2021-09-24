
#include "libstart/syscall.h"

void syscall_print(const char* string) {
    SYSCALL(SYSCALL_PRINT, (uintptr_t)string);
}

void syscall_exit(int status) {
    for (;;) {
        SYSCALL(SYSCALL_EXIT, status);
    }
}

void syscall_yield() {
    SYSCALL(SYSCALL_YIELD);
}

int syscall_fork() {
    return SYSCALL(SYSCALL_FORK);
}

void syscall_sleep(uint64_t nanoseconds) {
    SYSCALL(SYSCALL_FORK, nanoseconds);
}

