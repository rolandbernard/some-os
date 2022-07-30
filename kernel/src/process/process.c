
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "interrupt/com.h"
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
#include "task/harts.h"
#include "task/schedule.h"
#include "process/signals.h"
#include "process/syscall.h"
#include "process/types.h"
#include "task/types.h"
#include "task/spinlock.h"
#include "util/util.h"

extern void kernelTrapVector;
extern void kernelTrap;

static Pid next_pid = 1;

static Process* global_first = NULL;
SpinLock process_lock;

static bool basicProcessWait(Task* task) {
    int wait_pid = task->frame.regs[REG_ARGUMENT_1];
    ProcessWaitResult** current = &task->process->tree.waits;
    while (*current != NULL) {
        if (wait_pid == 0 || wait_pid == (*current)->pid) {
            writeInt(
                virtPtrForTask(task->frame.regs[REG_ARGUMENT_2], task),
                sizeof(int) * 8, (*current)->status
            );
            task->times.user_child_time += (*current)->user_time;
            task->times.system_child_time += (*current)->system_time;
            task->frame.regs[REG_ARGUMENT_0] = (*current)->pid;
            *current = (*current)->next;
            return true;
        } else {
            current = &(*current)->next;
        }
    }
    return false;
}

void finalProcessWait(Task* task) {
    // Called before waking up the process
    if (!basicProcessWait(task)) {
        task->frame.regs[REG_ARGUMENT_0] = -EINTR;;
    }
}

void executeProcessWait(Task* task) {
    lockSpinLock(&process_lock); 
    if (basicProcessWait(task)) {
        unlockSpinLock(&process_lock); 
        task->sched.state = ENQUABLE;
    } else {
        bool has_child = false;
        int wait_pid = task->frame.regs[REG_ARGUMENT_1];
        if (wait_pid == 0) {
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
            task->sched.state = WAIT_CHLD;
        } else {
            task->frame.regs[REG_ARGUMENT_0] = -ECHILD;
            task->sched.state = ENQUABLE;
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
        process->pid = allocateNewPid();
        if (parent == NULL) {
            process->memory.mem = createMemorySpace();
        } else {
            process->memory.mem = cloneMemorySpace(parent->memory.mem);
        }
        process->tree.parent = parent;
        process->resources.cwd = stringClone(process->resources.cwd);
        registerProcess(process);
    }
    return process;
}

Task* createTaskInProcess(Process* process, uintptr_t sp, uintptr_t gp, uintptr_t pc, Priority priority) {
    Task* task = createTask();
    if (task != NULL) {
        task->process = process;
        task->sched.priority = priority;
        task->sched.state = ENQUABLE;
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
    // TODO: For adding multiple task for a process this must be implemented correctly.
    // This fails for example if the task is running, is in a syscall, is waiting, etc.
    task->process->times.user_time += task->times.user_time;
    task->process->times.system_time += task->times.system_time;
    task->process->times.user_child_time += task->times.user_child_time;
    task->process->times.system_child_time += task->times.system_child_time;
    task->sched.state = TERMINATED;
    task->process = NULL;
}

void terminateAllProcessTasks(Process* process) {
    Task* current = process->tasks;
    while (current != NULL) {
        Task* next = current->proc_next;
        terminateProcessTask(current);
        current = next;
    }
    process->tasks = NULL;
}

static void freeProcessFiles(Process* process) {
    VfsFile* current = process->resources.files;
    while (current != NULL) {
        VfsFile* to_remove = current;
        current = current->next;
        to_remove->functions->close(to_remove, NULL, noop, NULL);
    }
    process->resources.next_fd = 0;
}

void deallocProcess(Process* process) {
    unregisterProcess(process);
    freeProcessFiles(process);
    if (process->pid != 0) {
        deallocMemorySpace(process->memory.mem);
    }
    dealloc(process);
}

void exitProcess(Process* process, Signal signal, int exit) {
    process->status = (exit & 0xff) | (signal << 8);
    terminateAllProcessTasks(process);
    deallocProcess(process);
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

void handleTaskWakeup(Task* task) {
    if (task->sched.state == WAIT_CHLD) {
        finalProcessWait(task);
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINTR;
    }
}

bool shouldTaskWakeup(Task* task) {
    return task->process->signals.signals != NULL;
}

