
#include <stdint.h>
#include <stddef.h>

#include "interrupt/syscall.h"

#define TABLE_SIZE 512

SyscallFunction syscall_table[TABLE_SIZE];

void registerSyscall(int kind, SyscallFunction function) {
    if (kind < TABLE_SIZE) {
        syscall_table[kind] = function;
    }
}

void runSyscall(Process* process) {
    uintptr_t kind = (uintptr_t)process->regs[9];
    if (kind < TABLE_SIZE && syscall_table[kind] != NULL) {
        process->regs[9] = syscall_table[kind](false, process, &(process->regs[10]));
    }
}

void runKernelSyscall(Process* process) {
    uintptr_t kind = (uintptr_t)process->regs[9];
    if (kind < TABLE_SIZE && syscall_table[kind] != NULL) {
        process->regs[9] = syscall_table[kind](true, process, &(process->regs[10]));
    }
}

