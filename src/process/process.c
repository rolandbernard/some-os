
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "process/process.h"

#include "error/log.h"
#include "error/panic.h"
#include "files/path.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/memspace.h"
#include "memory/pagetable.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"
#include "process/harts.h"
#include "process/schedule.h"
#include "process/signals.h"
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

static bool basicProcessWait(Process* process) {
    int wait_pid = process->frame.regs[REG_ARGUMENT_1];
    for (size_t i = 0; i < process->tree.wait_count; i++) {
        if (wait_pid == 0 || wait_pid == process->tree.waits[i].pid) {
            writeInt(
                virtPtrFor(process->frame.regs[REG_ARGUMENT_2], process->memory.mem),
                sizeof(int) * 8, process->tree.waits[i].status
            );
            process->times.user_child_time += process->tree.waits[i].user_time;
            process->times.system_child_time += process->tree.waits[i].system_time;
            process->frame.regs[REG_ARGUMENT_0] = process->tree.waits[i].pid;
            process->tree.wait_count--;
            memmove(process->tree.waits + i, process->tree.waits + i + 1, process->tree.wait_count - i);
            return true;
        }
    }
    return false;
}

void finalProcessWait(Process* process) {
    // Called before waking up the process
    if (!basicProcessWait(process)) {
        process->frame.regs[REG_ARGUMENT_0] = -EINTR;;
    }
}

void executeProcessWait(Process* process) {
    lockSpinLock(&process_lock); 
    if (basicProcessWait(process)) {
        unlockSpinLock(&process_lock); 
        moveToSchedState(process, ENQUEUEABLE);
        enqueueProcess(process);
    } else {
        bool has_child = false;
        int wait_pid = process->frame.regs[REG_ARGUMENT_1];
        if (wait_pid == 0) {
            has_child = process->tree.children != NULL;
        } else {
            Process* child = process->tree.children;
            while (child != NULL && !has_child) {
                if (child->pid == wait_pid) {
                    has_child = true;
                }
                child = child->tree.child_next;
            }
        }
        unlockSpinLock(&process_lock); 
        if (has_child) {
            moveToSchedState(process, WAIT_CHLD);
        } else {
            process->frame.regs[REG_ARGUMENT_0] = -ECHILD;
            moveToSchedState(process, ENQUEUEABLE);
        }
    }
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
            .user_time = process->times.user_time + process->times.user_child_time,
            .system_time = process->times.system_time + process->times.system_child_time,
        };
        parent->tree.waits[parent->tree.wait_count + process->tree.wait_count] = new_entry;
        parent->tree.wait_count = size;
        addSignalToProcess(parent, SIGCHLD);
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
    process->memory.mem = kernel_page_table;
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
    if (process != NULL) {
        process->pid = allocateNewPid();
        process->memory.mem = createMemorySpace();
        process->memory.stack = NULL;
        process->sched.priority = priority;
        process->sched.state = ENQUEUEABLE;
        process->tree.parent = parent;
        initTrapFrame(&process->frame, sp, gp, pc, process->pid, process->memory.mem);
        registerProcess(process);
    }
    return process;
}

Process* createChildUserProcess(Process* parent) {
    Process* process = zalloc(sizeof(Process));
    if (process != NULL) {
        process->pid = allocateNewPid();
        process->memory.mem = cloneMemorySpace(parent->memory.mem);
        process->memory.stack = NULL;
        process->sched.priority = parent->sched.priority;
        process->sched.state = ENQUEUEABLE;
        process->tree.parent = parent;
        process->resources.cwd = stringClone(process->resources.cwd);
        initTrapFrame(&process->frame, 0, 0, 0, process->pid, process->memory.mem);
        registerProcess(process);
    }
    return process;
}

static void freeProcess(Process* process) {
    if (process->sched.state != FREED) {
        process->sched.state = FREED;
        dealloc(process->memory.stack);
        process->memory.stack = NULL;
        if (process->pid != 0) {
            // If this is not a kernel process, free its page table.
            freeMemorySpace(process->memory.mem);
        }
        for (size_t i = 0; i < process->resources.fd_count; i++) {
            process->resources.filedes[i]->functions->close(
                process->resources.filedes[i], NULL, noop, NULL
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
        deallocMemorySpace(process->memory.mem);
    }
    dealloc(process);
}

void enterProcess(Process* process) {
    moveToSchedState(process, RUNNING);
    HartFrame* hart = getCurrentHartFrame();
    process->frame.hart = hart;
    if (hart != NULL) {
        hart->frame.regs[REG_STACK_POINTER] = (uintptr_t)hart->stack_top;
    }
    handlePendingSignals(process);
    process->times.entered = getTime();
    if (process->sched.state == RUNNING) {
        if (process->pid == 0) {
            enterKernelMode(&process->frame);
        } else {
            enterUserMode(&process->frame);
        }
    } else {
        enqueueProcess(process);
        runNextProcess();
    }
}

void dumpProcessInfo(Process* process) {
    if (process->frame.hart->idle_process == process) {
        logKernelMessage("[IDLE]\n");
    } else {
        logKernelMessage("pid %i\n", process->pid);
        logKernelMessage("regs: \n");
        for (int i = 1; i < 32; i += 4) {
            for (int j = i; j < i + 4 && j < 32; j++) {
                logKernelMessage("\tx%-2i %14lx", j, process->frame.regs[j - 1]);
            }
            logKernelMessage("\n");
        }
        logKernelMessage("satp %p \tpc %p\n", process->frame.satp, process->frame.pc);
        logKernelMessage("stack:\n");
        for (int i = 0; i < 128; i++) {
            intptr_t vaddr = process->frame.regs[REG_STACK_POINTER] + i * 8;
            uint64_t* maddr = (uint64_t*)virtToPhys(process->memory.mem, vaddr, false, false);
            if (maddr != NULL) {
                logKernelMessage("\t*%p(%p) = %14lx\n", vaddr, maddr, *maddr);
            }
        }
    }
}

int doForProcessWithPid(int pid, ProcessFindCallback callback, void* udata) {
    lockSpinLock(&process_lock);
    Process* current = global_first;
    while (current != NULL) {
        if (current->pid == pid) {
            unlockSpinLock(&process_lock);
            return callback(current, udata);
        }
        current = current->tree.global_next;
    }
    unlockSpinLock(&process_lock);
    return -ESRCH;
}

