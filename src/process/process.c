
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "error/log.h"
#include "error/panic.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/pagetable.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"
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

void executeProcessWait(Process* process) {
    int wait_pid = process->frame.regs[REG_ARGUMENT_1];
    for (size_t i = 0; i < process->tree.wait_count; i++) {
        if (wait_pid == 0 || wait_pid == process->tree.waits[i].pid) {
            writeInt(
                virtPtrFor(process->frame.regs[REG_ARGUMENT_2], process->memory.table),
                sizeof(int) * 8, process->tree.waits[i].status
            );
            process->frame.regs[REG_ARGUMENT_0] = process->tree.waits[i].pid;
            process->tree.wait_count--;
            memmove(process->tree.waits + i, process->tree.waits + i + 1, process->tree.wait_count - i);
            moveToSchedState(process, ENQUEUEABLE);
            enqueueProcess(process);
            return;
        }
    }
    moveToSchedState(process, WAIT_CHLD);
}

static void registerProcess(Process* process) {
    lockSpinLock(&process_lock); 
    process->tree.global_next = global_first;
    if (global_first != NULL) {
        global_first->tree.global_prev = process;
    }
    global_first = process;
    if (process->tree.parent != NULL) {
        if (process->tree.parent->tree.children != NULL) {
            process->tree.parent->tree.children->tree.child_prev = process;
        }
        process->tree.child_next = process->tree.parent->tree.children;
        process->tree.parent->tree.children = process;
    }
    unlockSpinLock(&process_lock); 
}

static void unregisterProcess(Process* process) {
    lockSpinLock(&process_lock); 
    // Add wait information to the parent
    if (process->tree.parent != NULL) {
        Process* parent = process->tree.parent;
        size_t size = process->tree.wait_count + 1 + parent->tree.wait_count;
        parent->tree.waits = krealloc(parent->tree.waits, size * sizeof(ProcessWaitResult));
        memcpy(
            parent->tree.waits + parent->tree.wait_count, process->tree.waits,
            process->tree.wait_count * sizeof(ProcessWaitResult)
        );
        ProcessWaitResult new_entry = {
            .pid = process->pid,
            .status = process->status,
        };
        parent->tree.waits[parent->tree.wait_count + process->tree.wait_count] = new_entry;
        parent->tree.wait_count = size;
        if (parent->sched.state == WAIT_CHLD) {
            executeProcessWait(parent);
        }
    }
    // Reparent children
    Process* child = process->tree.children;
    while (child != NULL) {
        child->tree.parent = process->tree.parent;
        if (process->tree.parent != NULL) {
            child->tree.child_next = process->tree.parent->tree.children;
            process->tree.parent->tree.children = child;
        }
        child = child->tree.child_next;
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
    registerProcess(process);
    return process;
}

Pid allocateNewPid() {
    lockSpinLock(&process_lock); 
    Pid pid = next_pid;
    next_pid++;
    unlockSpinLock(&process_lock); 
    return pid;
}

Process* createUserProcess(uintptr_t sp, uintptr_t gp, uintptr_t pc, Process* parent, Priority priority) {
    Process* process = zalloc(sizeof(Process));
    assert(process != NULL);
    process->pid = allocateNewPid();
    process->memory.table = createPageTable();
    process->memory.stack = NULL;
    process->sched.priority = priority;
    process->sched.state = ENQUEUEABLE;
    process->tree.parent = parent;
    initTrapFrame(&process->frame, sp, gp, pc, process->pid, process->memory.table);
    registerProcess(process);
    return process;
}

Process* createChildUserProcess(Process* parent) {
    Process* process = zalloc(sizeof(Process));
    assert(process != NULL);
    process->pid = allocateNewPid();
    process->memory.table = createPageTable();
    process->memory.stack = NULL;
    process->sched.priority = parent->sched.priority;
    process->sched.state = ENQUEUEABLE;
    process->tree.parent = parent;
    initTrapFrame(&process->frame, 0, 0, parent->frame.pc, process->pid, process->memory.table);
    memcpy(&process->frame.fregs, &parent->frame.fregs, sizeof(parent->frame.fregs));
    memcpy(&process->frame.regs, &parent->frame.regs, sizeof(parent->frame.regs));
    registerProcess(process);
    return process;
}

static void freeProcess(Process* process) {
    if (process->sched.state != FREED) {
        process->sched.state = FREED;
        dealloc(process->memory.stack);
        process->memory.stack = NULL;
        if (process->pid != 0) {
            // If this is not a kernel process, free its page table.
            unmapAllPagesAndFreeUsers(process->memory.table);
        }
        for (size_t i = 0; i < process->resources.fd_count; i++) {
            process->resources.filedes[i]->functions->close(
                process->resources.filedes[i], 0, 0, noop, NULL
            );
        }
        dealloc(process->resources.filedes);
        process->resources.fd_count = 0;
        process->resources.next_fd = 0;
    }
}

void deallocProcess(Process* process) {
    unregisterProcess(process);
    freeProcess(process);
    if (process->pid != 0) {
        deallocPage(process->memory.table);
    }
    dealloc(process);
}

void enterProcess(Process* process) {
    initTimerInterrupt();
    moveToSchedState(process, RUNNING);
    HartFrame* hart = getCurrentHartFrame();
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

void dumpProcessInfo(Process* process) {
    if (process->frame.hart->idle_process == process) {
        KERNEL_LOG("[IDLE]");
    } else {
        KERNEL_LOG("pid %i", process->pid);
        KERNEL_LOG("regs: ");
        for (int i = 1; i < 32; i += 4) {
            for (int j = i; j < i + 4 && j < 32; j++) {
                logKernelMessage("\tx%-2i %14lx", j, process->frame.regs[j - 1]);
            }
            logKernelMessage("\n");
        }
        KERNEL_LOG("satp %p \tpc %p", process->frame.satp, process->frame.pc);
        KERNEL_LOG("stack: ");
        for (int i = 0; i < 128; i++) {
            intptr_t vaddr = process->frame.regs[REG_STACK_POINTER] + i * 8;
            uint64_t* maddr = (uint64_t*)virtToPhys(process->memory.table, vaddr);
            if (maddr != NULL) {
                KERNEL_LOG("\t*%p(%p) = %14lx", vaddr, maddr, *maddr);
            }
        }
    }
}

