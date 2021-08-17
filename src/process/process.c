
#include <stddef.h>

#include "process/process.h"

#include "error/panic.h"
#include "interrupt/trap.h"
#include "interrupt/syscall.h"

void initProcess(Process* process, void* stack_top, void* globals, void* start) {
    process->regs[0] = (void*)panic;
    process->regs[1] = stack_top;
    process->regs[2] = globals;
    process->pc = start;
    process->stack_top = stack_top;
    process->globals = globals;;
    process->state = READY;
}

void enterProcessAsUser(Process* process) {
    process->state = RUNNING;
    enterUserMode(process, process->pc);
}

void enterProcessAsKernel(Process* process) {
    process->state = RUNNING;
    enterKernelMode(process, process->pc);
}

void* exitSyscall(Process* process, SyscallArgs args) {
    process->state = TERMINATED;
    return NULL;
}

Error initProcessSystem() {
    registerSyscall(1, exitSyscall);
    return simpleError(SUCCESS);
}

