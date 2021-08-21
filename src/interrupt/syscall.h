#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "process/process.h"

// Simple wrapper around ecall
void* syscall(int kind, ...);

typedef uintptr_t SyscallArgs[7];
typedef uintptr_t (*SyscallFunction)(Process* process, SyscallArgs args);

// Register a syscall at the given kind
void registerSyscall(int kind, SyscallFunction function);

// Run a syscall for the given process. Return and extract arguments from the process registers.
void runSyscall(Process* process);

#endif
