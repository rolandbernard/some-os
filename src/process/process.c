
#include <stddef.h>

#include "process/process.h"

#include "error/panic.h"
#include "error/log.h"
#include "memory/virtmem.h"
#include "interrupt/trap.h"
#include "interrupt/syscall.h"

void initProcess(Process* process, uintptr_t stack_top, uintptr_t globals, uintptr_t start) {
    process->frame.regs[0] = (uintptr_t)panic;
    process->frame.regs[1] = stack_top;
    process->frame.regs[2] = globals;
    process->frame.pc = start;
    process->stack_top = stack_top;
    process->globals = globals;;
    process->state = READY;
    process->table = NULL;
    process->pid = 1;
}

void enterProcess(Process* process) {
    process->state = RUNNING;
    if (process->is_kernel) {
    enterKernelMode(&process->frame);
    } else {
        enterUserMode(&process->frame);
    }
}

uintptr_t exitSyscall(bool is_kernel, Process* process, SyscallArgs args) {
    process->state = TERMINATED;
    return 0;
}

Error initProcessSystem() {
    registerSyscall(1, exitSyscall);
    return simpleError(SUCCESS);
}

