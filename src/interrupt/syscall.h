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
    SYSCALL_READDIR = 21,
    SYSCALL_GETPID = 22,
    SYSCALL_GETPPID = 23,
    SYSCALL_WAIT = 24,
    SYSCALL_SBRK = 25,
    SYSCALL_PROTECT = 26,
    SYSCALL_SIGACTION = 27,
    SYSCALL_SIGRETURN = 28,
    SYSCALL_KILL = 29,
    SYSCALL_GETUID = 30,
    SYSCALL_GETGID = 31,
    SYSCALL_SETUID = 32,
    SYSCALL_SETGID = 33,
    SYSCALL_CHDIR = 34,
    SYSCALL_GETCWD = 35,
    SYSCALL_PIPE = 36,
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
