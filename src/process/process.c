
#include <stddef.h>
#include <assert.h>

#include "memory/pagetable.h"

#include "process/process.h"
#include "error/panic.h"
#include "error/log.h"
#include "memory/virtmem.h"
#include "interrupt/trap.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"

extern void kernelTrapVector;
extern void kernelTrap;
extern void __data_start;
extern void __data_end;

static Pid next_pid = 1;

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, HartFrame* hart, uintptr_t asid, PageTable* table) {
    frame->hart = hart;
    frame->regs[0] = 0;
    frame->regs[1] = sp;
    frame->regs[2] = gp;
    frame->pc = pc;
    frame->satp = satpForMemory(asid, table);
}

void initDefaultProcess(Process* process, uintptr_t stack_top, uintptr_t globals, uintptr_t start) {
    Pid pid = next_pid;
    next_pid++;
    // TODO: create page table
    PageTable* vmem = NULL;
    initTrapFrame(&process->frame, stack_top, globals, start, NULL, pid, vmem);
    process->state = READY;
    process->table = vmem;
    process->pid = pid;
}

void enterProcess(Process* process) {
    initTimerInterrupt();
    process->state = RUNNING;
    HartFrame* hart = process->frame.hart;
    if (hart != NULL) {
        hart->frame.regs[1] = (uintptr_t)hart->stack_top;
        hart->frame.regs[2] = (uintptr_t)hart->globals;
    }
    if (process->pid == 0) {
        enterKernelMode(&process->frame);
    } else {
        enterUserMode(&process->frame);
    }
}

uintptr_t exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->state = TERMINATED;
    return 0;
}

Error initProcessSystem() {
    registerSyscall(1, exitSyscall);
    return simpleError(SUCCESS);
}

