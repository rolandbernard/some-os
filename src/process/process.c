
#include <stddef.h>
#include <assert.h>

#include "error/log.h"
#include "error/panic.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "process/harts.h"
#include "process/process.h"

#define STACK_SIZE (1 << 16)

extern void kernelTrapVector;
extern void kernelTrap;
extern void __data_start;
extern void __data_end;

static Pid next_pid = 1;

static Process* global_first = NULL;

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, HartFrame* hart, uintptr_t asid, PageTable* table) {
    frame->hart = hart;
    frame->regs[0] = 0;
    frame->regs[1] = sp;
    frame->regs[2] = gp;
    frame->pc = pc;
    frame->satp = satpForMemory(asid, table);
}

Process* createKernelProcess(void* start, Priority priority) {
    Process* process = zalloc(sizeof(Process));
    process->table = kernel_page_table;
    process->priority = priority;
    process->state = READY;
    process->stack = kalloc(STACK_SIZE);
    initTrapFrame(
        &process->frame, (uintptr_t)process->stack, (uintptr_t)getKernelGlobalPointer(),
        (uintptr_t)start, getCurrentHartFrame(), 0, kernel_page_table
    );
    process->global_next = global_first;
    global_first = process;
    return process;
}

Process* createEmptyUserProcess(uintptr_t sp, uintptr_t gp, uintptr_t pc, Pid ppid, Priority priority) {
    Process* process = zalloc(sizeof(Process));
    Pid pid = next_pid;
    next_pid++;
    process->table = createPageTable();
    process->pid = pid;
    process->ppid = ppid;
    process->priority = priority;
    process->state = READY;
    process->stack = NULL;
    initTrapFrame(&process->frame, sp, gp, pc, getCurrentHartFrame(), pid, process->table);
    process->global_next = global_first;
    global_first = process;
    return process;
}

void freeProcess(Process* process) {
    // TODO
}

void enterProcess(Process* process) {
    initTimerInterrupt();
    process->state = RUNNING;
    HartFrame* hart = getCurrentHartFrame();
    if (hart != process->frame.hart && process->pid != 0) {
        // If this process was moved between harts
        addressTranslationFence(process->pid);
    }
    process->frame.hart = hart;
    if (hart != NULL) {
        hart->frame.regs[1] = (uintptr_t)hart->stack_top;
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

