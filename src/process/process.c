
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
#include "process/types.h"
#include "util/spinlock.h"

extern void kernelTrapVector;
extern void kernelTrap;
extern void __data_start;
extern void __data_end;

static Pid next_pid = 1;
static Tid next_tid = 1;

static SpinLock process_lock;
static Process* global_first = NULL;

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
    process->table = kernel_page_table;
    process->priority = priority;
    process->state = ENQUEUEABLE;
    process->stack = kalloc(stack_size);
    assert(process->stack != NULL);
    initTrapFrame(
        &process->frame, (uintptr_t)process->stack + stack_size,
        (uintptr_t)getKernelGlobalPointer(), (uintptr_t)start, 0, kernel_page_table
    );
    lockSpinLock(&process_lock); 
    process->tid = next_tid;
    next_tid++;
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
    assert(process != NULL);
    lockSpinLock(&process_lock); 
    process->pid = next_pid;
    next_pid++;
    unlockSpinLock(&process_lock); 
    process->table = createPageTable();
    process->parent = parent;
    process->priority = priority;
    process->state = ENQUEUEABLE;
    process->stack = NULL;
    initTrapFrame(&process->frame, sp, gp, pc, process->pid, process->table);
    lockSpinLock(&process_lock); 
    process->tid = next_tid;
    next_tid++;
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
    if (process->state != FREED) {
        process->state = FREED;
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
        if ((*child)->state == FREED) {
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
        hart->frame.regs[REG_STACK_POINTER] = (uintptr_t)hart->stack_top;
    }
    if (process->pid == 0) {
        enterKernelMode(&process->frame);
    } else {
        enterUserMode(&process->frame);
    }
}

