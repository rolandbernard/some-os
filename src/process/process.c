
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
#include "process/syscall.h"
#include "util/spinlock.h"

#define STACK_SIZE (1 << 16)

extern void kernelTrapVector;
extern void kernelTrap;
extern void __data_start;
extern void __data_end;

static Pid next_pid = 1;

static SpinLock process_lock;
static Process* global_first = NULL;

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, HartFrame* hart, uintptr_t asid, PageTable* table) {
    frame->hart = hart;
    frame->regs[0] = (uintptr_t)exit;
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
        &process->frame, (uintptr_t)process->stack + STACK_SIZE,
        (uintptr_t)getKernelGlobalPointer(), (uintptr_t)start, getCurrentHartFrame(), 0,
        kernel_page_table
    );
    lockSpinLock(&process_lock); 
    process->global_next = global_first;
    if (global_first != NULL) {
        global_first->global_prev = process;
    }
    global_first = process;
    unlockSpinLock(&process_lock); 
    return process;
}

Process* createEmptyUserProcess(uintptr_t sp, uintptr_t gp, uintptr_t pc, Process* parent, Priority priority) {
    Process* process = zalloc(sizeof(Process));
    Pid pid = next_pid;
    next_pid++;
    process->table = createPageTable();
    process->pid = pid;
    process->parent = parent;
    process->priority = priority;
    process->state = READY;
    process->stack = NULL;
    initTrapFrame(&process->frame, sp, gp, pc, getCurrentHartFrame(), pid, process->table);
    lockSpinLock(&process_lock); 
    process->global_next = global_first;
    if (global_first != NULL) {
        global_first->global_prev = process;
    }
    global_first = process;
    if (parent != NULL) {
        process->child_next = parent->children;
        parent->children = process;
    }
    unlockSpinLock(&process_lock); 
    return process;
}

void freeProcess(Process* process) {
    if (process->state != KILLED) {
        process->state = KILLED;
        lockSpinLock(&process_lock); 
        if (process->global_prev == NULL) {
            global_first = process->global_next;
        } else {
            process->global_prev->global_next = process->global_next;
        }
        if (process->global_next != NULL) {
            process->global_next->global_prev = process->global_prev;
        }
        // Reparent children
        Process* child = process->children;
        while (child != NULL) {
            child->parent = process->parent;
            if (process->parent != NULL) {
                child->child_next = process->parent->children;
                process->parent->children = child;
            }
        }
        unlockSpinLock(&process_lock); 
        dealloc(process->stack);
        if (process->pid != 0) {
            // If this is not a kernel process, free its page table.
            unmapAllPagesAndFreeUsers(process->table);
            deallocPage(process->table);
        }
        if (process->parent == NULL) {
            // If we have no parent, no one can wait for the child.
            dealloc(process);
        }
    }
}

Pid freeKilledChild(Process* parent, uint64_t* status) {
    lockSpinLock(&process_lock); 
    Process** child = &parent->children;
    while (*child != NULL) {
        if ((*child)->state == KILLED) {
            Process* ret = *child;
            *child = ret->child_next;
            unlockSpinLock(&process_lock); 
            Pid pid = ret->pid;
            *status = ret->status;
            dealloc(ret);
            return pid;
        }
        child = &(*child)->child_next;
    }
    unlockSpinLock(&process_lock); 
    return 0;
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

