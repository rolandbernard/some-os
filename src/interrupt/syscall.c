
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
    [SYSCALL_FORK] = forkSyscall,
    [SYSCALL_SLEEP] = sleepSyscall,
};

void registerSyscall(int kind, SyscallFunction function) {
    if (kind < TABLE_SIZE) {
        syscall_table[kind] = function;
    }
}

void runSyscall(TrapFrame* frame, bool is_kernel) {
    uintptr_t kind = (uintptr_t)frame->regs[REG_ARGUMENT_0];
    if (kind < TABLE_SIZE && syscall_table[kind] != NULL) {
        frame->regs[REG_ARGUMENT_0] =
            syscall_table[kind](is_kernel, frame, &(frame->regs[REG_ARGUMENT_1]));
    }
}
