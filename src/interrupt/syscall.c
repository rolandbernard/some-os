
#include <stdint.h>
#include <stddef.h>

#include "error/log.h"
#include "interrupt/syscall.h"
#include "process/syscall.h"

#define TABLE_SIZE 512

SyscallFunction syscall_table[TABLE_SIZE] = {
    [SYSCALL_PRINT] = printSyscall,
    [SYSCALL_EXIT] = exitSyscall,
    [SYSCALL_YIELD] = yieldSyscall,
};

void registerSyscall(int kind, SyscallFunction function) {
    if (kind < TABLE_SIZE) {
        syscall_table[kind] = function;
    }
}

void runSyscall(TrapFrame* frame, bool is_kernel) {
    uintptr_t kind = (uintptr_t)frame->regs[9];
    if (kind < TABLE_SIZE && syscall_table[kind] != NULL) {
        frame->regs[9] = syscall_table[kind](is_kernel, frame, &(frame->regs[10]));
    }
}

