#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "process/process.h"

typedef enum {
    SYSCALL_PRINT = 0,
    SYSCALL_EXIT = 1,
    SYSCALL_YIELD = 2,
    SYSCALL_FORK = 3,
    SYSCALL_SLEEP = 4,
} Syscalls;

// Simple wrapper around ecall
void* syscall(int kind, ...);

typedef uintptr_t SyscallArgs[7];
typedef uintptr_t (*SyscallFunction)(bool is_kernel, TrapFrame* process, SyscallArgs args);

// Register a syscall at the given kind
void registerSyscall(int kind, SyscallFunction function);

// Run a syscall for the given process. Return and extract arguments from the process registers.
void runSyscall(TrapFrame* frame, bool is_kernel);

#endif
