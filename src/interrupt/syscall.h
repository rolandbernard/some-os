#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "process/process.h"

typedef enum {
    SYSCALL_PRINT = 0,
    SYSCALL_EXIT = 1,
    SYSCALL_YIELD = 2,
    SYSCALL_FORK = 3,
    SYSCALL_SLEEP = 4,
    SYSCALL_OPEN = 5,
    SYSCALL_LINK = 6,
    SYSCALL_UNLINK = 7,
    SYSCALL_RENAME = 8,
    SYSCALL_CLOSE = 9,
    SYSCALL_READ = 10,
    SYSCALL_WRITE = 11,
    SYSCALL_SEEK = 12,
    SYSCALL_STAT = 13,
    SYSCALL_DUP = 14,
    SYSCALL_TRUNC = 15,
    SYSCALL_CHMOD = 16,
    SYSCALL_CHOWN = 17,
    SYSCALL_MOUNT = 18,
    SYSCALL_UMOUNT = 19,
    SYSCALL_EXECVE = 20,
} Syscalls;

// Simple wrapper around ecall
intptr_t syscall(int kind, ...);

typedef uintptr_t SyscallArgs[7];
typedef void (*SyscallFunction)(bool is_kernel, TrapFrame* process, SyscallArgs args);

// Register a syscall at the given kind
void registerSyscall(int kind, SyscallFunction function);

// Run a syscall for the given process. Return and extract arguments from the process registers.
void runSyscall(TrapFrame* frame, bool is_kernel);

char* copyStringFromSyscallArgs(Process* process, uintptr_t ptr);

#endif
