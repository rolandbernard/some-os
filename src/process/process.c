
#include <stddef.h>
#include <assert.h>

#include "error/log.h"
#include "error/panic.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/pagetable.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "process/harts.h"
#include "process/process.h"
#include "process/schedule.h"
#include "process/syscall.h"
#include "process/types.h"
#include "util/spinlock.h"
#include "util/util.h"

extern void kernelTrapVector;
extern void kernelTrap;
extern void __data_start;
extern void __data_end;

static Pid next_pid = 1;

static Process* global_first = NULL;
SpinLock process_lock;

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, uintptr_t asid, PageTable* table) {
    frame->hart = NULL; // Set to NULL for now. Will be set when enqueuing
    frame->regs[REG_RETURN_ADDRESS] = (uintptr_t)exit;
    frame->regs[REG_STACK_POINTER] = sp;
    frame->regs[REG_GLOBAL_POINTER] = gp;
    frame->pc = pc;
    frame->satp = satpForMemory(asid, table);
}

Process* createKernelProcess(void* start, Priority priority, size_t stack_size) {
    Process* process = zalloc(sizeof(Process));
    assert(process != NULL);
    process->memory.table = kernel_page_table;
    process->memory.stack = kalloc(stack_size);
    assert(process->memory.stack != NULL);
    process->sched.priority = priority;
    process->sched.state = ENQUEUEABLE;
    initTrapFrame(
        &process->frame, (uintptr_t)process->memory.stack + stack_size,
        (uintptr_t)getKernelGlobalPointer(), (uintptr_t)start, 0, kernel_page_table
    );
    lockSpinLock(&process_lock); 
    process->tree.global_next = global_first;
    if (global_first != NULL) {
        global_first->tree.global_prev = process;
    }
    global_first = process;
    unlockSpinLock(&process_lock); 
    return process;
}

Pid allocateNewPid() {
    lockSpinLock(&process_lock); 
    Pid pid = next_pid;
    next_pid++;
    unlockSpinLock(&process_lock); 
    return pid;
}

Process* createEmptyUserProcess(uintptr_t sp, uintptr_t gp, uintptr_t pc, Process* parent, Priority priority) {
    Process* process = zalloc(sizeof(Process));
    assert(process != NULL);
    process->pid = allocateNewPid();
    process->memory.table = createPageTable();
    process->memory.stack = NULL;
    process->sched.priority = priority;
    process->sched.state = ENQUEUEABLE;
    initTrapFrame(&process->frame, sp, gp, pc, process->pid, process->memory.table);
    lockSpinLock(&process_lock); 
    process->tree.global_next = global_first;
    if (global_first != NULL) {
        global_first->tree.global_prev = process;
    }
    global_first = process;
    if (parent != NULL) {
        process->tree.parent = parent;
        if (parent->tree.children != NULL) {
            parent->tree.children->tree.child_prev = process;
        }
        process->tree.child_next = parent->tree.children;
        parent->tree.children = process;
    }
    unlockSpinLock(&process_lock); 
    return process;
}

void freeProcess(Process* process) {
    if (process->sched.state != FREED) {
        process->sched.state = FREED;
        dealloc(process->memory.stack);
        process->memory.stack = NULL;
        if (process->pid != 0) {
            // If this is not a kernel process, free its page table.
            unmapAllPagesAndFreeUsers(process->memory.table);
        }
        for (size_t i = 0; i < process->resources.fd_count; i++) {
            process->resources.files[i]->functions->close(
                process->resources.files[i], 0, 0, noop, NULL
            );
        }
        dealloc(process->resources.fds);
        dealloc(process->resources.files);
        process->resources.fd_count = 0;
        process->resources.next_fd = 0;
    }
}

void deallocProcess(Process* process) {
    freeProcess(process);
    lockSpinLock(&process_lock); 
    // Reparent children
    Process* child = process->tree.children;
    while (child != NULL) {
        child->tree.parent = process->tree.parent;
        if (process->tree.parent != NULL) {
            child->tree.child_next = process->tree.parent->tree.children;
            process->tree.parent->tree.children = child;
        }
    }
    if (process->tree.global_prev == NULL) {
        global_first = process->tree.global_next;
    } else {
        process->tree.global_prev->tree.global_next = process->tree.global_next;
    }
    if (process->tree.global_next != NULL) {
        process->tree.global_next->tree.global_prev = process->tree.global_prev;
    }
    unlockSpinLock(&process_lock); 
    if (process->pid != 0) {
        deallocPage(process->memory.table);
    }
    dealloc(process);
}

void enterProcess(Process* process) {
    initTimerInterrupt();
    moveToSchedState(process, RUNNING);
    HartFrame* hart = getCurrentHartFrame();
    if (hart != process->frame.hart && process->pid != 0) {
        // If this process was moved between harts
        addressTranslationFence(process->pid);
    }
    process->frame.hart = hart;
    if (hart != NULL) {
        hart->frame.regs[REG_STACK_POINTER] = (uintptr_t)hart->stack_top;
    }
    if (process->pid == 0) {
        enterKernelMode(&process->frame);
    } else {
        enterUserMode(&process->frame);
    }
}

