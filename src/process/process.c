
#include <stddef.h>

#include "memory/pagetable.h"

#include "process/process.h"
#include "error/panic.h"
#include "error/log.h"
#include "memory/virtmem.h"
#include "interrupt/trap.h"
#include "interrupt/syscall.h"

extern void kernelTrapVector;
extern void kernelTrap;
extern void __data_start;
extern void __data_end;

static Pid next_pid = 1;

void initDefaultProcess(Process* process, uintptr_t stack_top, uintptr_t globals, uintptr_t start) {
    Pid pid = next_pid;
    next_pid++;
    // TODO: create page table
    PageTable* vmem = NULL;
    process->frame.hart = NULL;
    process->frame.regs[0] = (uintptr_t)panic;
    process->frame.regs[1] = stack_top;
    process->frame.regs[2] = globals;
    process->frame.pc = start;
    process->frame.satp = satpForMemory(next_pid, vmem);
    process->stack_top = stack_top;
    process->globals = globals;;
    process->state = READY;
    process->table = vmem;
    process->pid = pid;
}

void enterProcess(Process* process) {
    process->state = RUNNING;
    if (process->pid == 0) {
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

