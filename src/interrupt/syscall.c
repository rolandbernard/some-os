
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

void runSyscall(TrapFrame* frame) {
    uintptr_t kind = (uintptr_t)frame->regs[9];
    if (kind < TABLE_SIZE && syscall_table[kind] != NULL) {
        frame->regs[9] = syscall_table[kind](false, frame, &(frame->regs[10]));
    }
}

void runKernelSyscall(TrapFrame* frame) {
    uintptr_t kind = (uintptr_t)frame->regs[9];
    if (kind < TABLE_SIZE && syscall_table[kind] != NULL) {
        frame->regs[9] = syscall_table[kind](true, frame, &(frame->regs[10]));
    }
}

