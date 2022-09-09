
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "process/process.h"

#include "error/log.h"
#include "error/panic.h"
#include "files/path.h"
#include "files/process.h"
#include "interrupt/com.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/memspace.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"
#include "process/signals.h"
#include "process/syscall.h"
#include "process/types.h"
#include "task/harts.h"
#include "task/schedule.h"
#include "task/spinlock.h"
#include "task/syscall.h"
#include "task/types.h"
#include "util/util.h"

extern void kernelTrapVector;
extern void kernelTrap;

static Pid next_pid = 1;

static Process* global_first = NULL;
SpinLock process_lock;

static bool basicProcessWait(Task* task) {
    Pid wait_pid = task->frame.regs[REG_ARGUMENT_1];
    ProcessWaitResult** current = &task->process->tree.waits;
    while (*current != NULL) {
        ProcessWaitResult* wait = *current;
        if (wait_pid <= 0 || wait_pid == (*current)->pid) {
            writeInt(
                virtPtrForTask(task->frame.regs[REG_ARGUMENT_2], task),
                sizeof(int) * 8, wait->status
            );
            task->times.user_child_time += wait->user_time;
            task->times.system_child_time += wait->system_time;
            task->frame.regs[REG_ARGUMENT_0] = wait->pid;
            *current = wait->next;
            dealloc(wait);
            return true;
        } else {
            current = &wait->next;
        }
    }
    return false;
}

void executeProcessWait(Task* task) {
    lockSpinLock(&process_lock); 
    if (basicProcessWait(task)) {
        unlockSpinLock(&process_lock); 
        moveTaskToState(task, ENQUABLE);
    } else {
        bool has_child = false;
        Pid wait_pid = task->frame.regs[REG_ARGUMENT_1];
        if (wait_pid <= 0) {
            has_child = task->process->tree.children != NULL;
        } else {
            Process* child = task->process->tree.children;
            while (child != NULL && !has_child) {
                if (child->pid == wait_pid) {
                    has_child = true;
                }
                child = child->tree.child_next;
            }
        }
        unlockSpinLock(&process_lock); 
        if (has_child) {
            task->frame.regs[REG_ARGUMENT_0] = -EINTR;
            moveTaskToState(task, WAIT_CHLD);
        } else {
            task->frame.regs[REG_ARGUMENT_0] = -ECHILD;
            moveTaskToState(task, ENQUABLE);
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
    if (process->tree.parent != NULL) {
        // Add wait information to the parent
        Process* parent = process->tree.parent;
        ProcessWaitResult* current = process->tree.waits;
        while (current != NULL) {
            ProcessWaitResult* wait = current;
            current = wait->next;
            wait->next = parent->tree.waits;
            parent->tree.waits = wait->next;
        }
        ProcessWaitResult* new_entry = kalloc(sizeof(ProcessWaitResult));
        new_entry->pid = process->pid;
        new_entry->status = process->status;
        new_entry->user_time = process->times.user_time + process->times.user_child_time;
        new_entry->system_time = process->times.system_time + process->times.system_child_time;
        new_entry->next = parent->tree.waits;
        parent->tree.waits = new_entry;
        addSignalToProcess(parent, SIGCHLD);
        // Remove child from parent
        if (process->tree.child_prev == NULL) {
            parent->tree.children = process->tree.child_next;
        } else {
            process->tree.child_prev->tree.child_next = process->tree.child_next;
        }
    } else {
        // Free pending waits
        ProcessWaitResult* current = process->tree.waits;
        while (current != NULL) {
            ProcessWaitResult* wait = current;
            current = wait->next;
            dealloc(wait);
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

Pid allocateNewPid() {
    lockSpinLock(&process_lock); 
    Pid pid = next_pid;
    next_pid++;
    unlockSpinLock(&process_lock); 
    return pid;
}

Process* createUserProcess(Process* parent) {
    Process* process = zalloc(sizeof(Process));
    if (process != NULL) {
        if (parent != NULL) {
            // Copy session id and process group id
            process->sid = parent->sid;
            process->pgid = parent->pgid;
            // Copy signal handler data, but not pending signals
            process->signals.current_signal = parent->signals.current_signal;
            process->signals.restore_frame = parent->signals.restore_frame;
            memcpy(
                process->signals.handlers, parent->signals.handlers,
                sizeof(parent->signals.handlers)
            );
            // Copy resource information
            lockTaskLock(&parent->resources.lock);
            process->resources.cwd = stringClone(parent->resources.cwd);
            process->resources.umask = parent->resources.umask;
            unlockTaskLock(&parent->resources.lock);
            lockSpinLock(&parent->user.lock);
            process->user.ruid = parent->user.ruid;
            process->user.rgid = parent->user.rgid;
            process->user.suid = parent->user.suid;
            process->user.sgid = parent->user.sgid;
            process->user.euid = parent->user.euid;
            process->user.egid = parent->user.egid;
            unlockSpinLock(&parent->user.lock);
            process->memory.start_brk = parent->memory.start_brk;
            process->memory.brk = parent->memory.brk;
            process->memory.mem = cloneMemorySpace(parent->memory.mem);
            // Copy files
            forkFileDescriptors(process, parent);
        } else {
            process->memory.mem = createMemorySpace();
            process->resources.cwd = stringClone("/");
        }
        process->pid = allocateNewPid();
        process->tree.parent = parent;
        registerProcess(process);
    }
    return process;
}

Task* createTaskInProcess(Process* process, uintptr_t sp, uintptr_t gp, uintptr_t pc, Priority priority) {
    Task* task = createTask();
    if (task != NULL) {
        task->process = process;
        task->sched.priority = priority;
        moveTaskToState(task, ENQUABLE);
        initTrapFrame(&task->frame, sp, gp, pc, process->pid, process->memory.mem);
        addTaskToProcess(process, task);
    }
    return task;
}

void addTaskToProcess(Process* process, Task* task) {
    lockSpinLock(&process->lock);
    task->process = process;
    task->proc_next = process->tasks;
    process->tasks = task;
    unlockSpinLock(&process->lock);
}

static void terminateProcessTask(Task* task) {
    moveTaskToState(task, TERMINATED);
    sendMessageToAll(YIELD_TASK, task);
}

void terminateAllProcessTasksBut(Process* process, Task* keep) {
    Task* current = process->tasks;
    while (current != NULL) {
        if (current != keep) {
            terminateProcessTask(current);
        }
        current = current->proc_next;
    }
}

void terminateAllProcessTasks(Process* process) {
    terminateAllProcessTasksBut(process, NULL);
}

static void processFinalizeTask(Process* process) {
    // This must be a task because it might include blocking operations.
    unregisterProcess(process);
    closeAllProcessFiles(process);
    if (process->pid != 0) {
        deallocMemorySpace(process->memory.mem);
    }
    dealloc(process);
    leave();
}

void deallocProcess(Process* process) {
    if (getCurrentTask() == NULL) {
        Task* syscall_task = createKernelTask(processFinalizeTask, HART_STACK_SIZE, DEFAULT_PRIORITY - 10);
        syscall_task->frame.regs[REG_ARGUMENT_0] = (uintptr_t)process;
        enqueueTask(syscall_task);
    } else {
        processFinalizeTask(process);
    }
}

void exitProcess(Process* process, Signal signal, int exit) {
    process->status = (exit & 0xff) | (signal << 8);
    terminateAllProcessTasks(process);
}

void removeProcessTask(Task* task) {
    assert(task->process != NULL);
    Process* process = task->process;
    lockSpinLock(&process->lock);
    Task** curr = &process->tasks;
    while (*curr != NULL) {
        if (*curr == task) {
            *curr = task->proc_next;
        } else {
            curr = &(*curr)->proc_next;
        }
    }
    process->times.user_time += task->times.user_time;
    process->times.system_time += task->times.system_time;
    process->times.user_child_time += task->times.user_child_time;
    process->times.system_child_time += task->times.system_child_time;
    task->process = NULL;
    if (process->tasks == NULL) {
        unlockSpinLock(&process->lock);
        deallocProcess(process);
    } else {
        unlockSpinLock(&process->lock);
    }
}

int doForProcessWithPid(Pid pid, ProcessFindCallback callback, void* udata) {
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

void doForAllProcess(ProcessFindCallback callback, void* udata) {
    lockSpinLock(&process_lock);
    Process* current = global_first;
    while (current != NULL) {
        callback(current, udata);
        current = current->tree.global_next;
    }
    unlockSpinLock(&process_lock);
}

void handleProcessTaskWakeup(Task* task) {
    if (task->sched.state == WAIT_CHLD) {
        basicProcessWait(task);
    }
}

bool shouldTaskWakeup(Task* task) {
    return task->process->signals.signals != NULL;
}

