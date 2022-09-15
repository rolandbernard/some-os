
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

#define WNOHANG     0b001
#define WUNTRACED   0b010
#define WCONTINUED  0b100

#define STATUS_STOPPED 1
#define STATUS_RESUMED 2

static Error basicProcessWait(Task* task) {
    size_t found = 0;
    Pid pid = task->frame.regs[REG_ARGUMENT_1];
    int flags = task->frame.regs[REG_ARGUMENT_3];
    Process** current = &task->process->tree.children;
    while (*current != NULL) {
        Process* child = *current;
        if (
            pid == -1
            || (pid == 0 && task->process->pgid == child->pgid)
            || (pid < -1 && task->process->pgid == -pid)
            || (pid > 0 && child->pid == pid)
        ) {
            found++;
            lockSpinLock(&child->lock);
            if (
                child->tasks == NULL
                || ((flags & WUNTRACED) != 0 && (child->status >> 16) == STATUS_STOPPED)
                || ((flags & WCONTINUED) != 0 && (child->status >> 16) == STATUS_RESUMED)
            ) {
                unlockSpinLock(&child->lock);
                writeInt(
                    virtPtrForTask(task->frame.regs[REG_ARGUMENT_2], task),
                    sizeof(int) * 8, child->status
                );
                child->status &= 0xffff;
                task->times.user_child_time += child->times.user_time + child->times.user_child_time;
                task->times.system_child_time += child->times.system_time + child->times.system_child_time;
                task->frame.regs[REG_ARGUMENT_0] = child->pid;
                *current = child->tree.child_next;
                if (*current != NULL) {
                    (*current)->tree.child_prev = child->tree.child_prev;
                }
                deallocProcess(child);
                return simpleError(SUCCESS);
            } else {
                unlockSpinLock(&child->lock);
            }
        }
        current = &child->tree.child_next;
    }
    return simpleError(found != 0 ? ((flags & WNOHANG) != 0 ? EAGAIN : EINTR) : ECHILD);
}

bool handleProcessWaitWakeup(Task* task) {
    lockSpinLock(&task->process->lock); 
    if (task->process->signals.signals != NULL) {
        lockSpinLock(&process_lock); 
        Error err = basicProcessWait(task);
        unlockSpinLock(&process_lock); 
        if (isError(err)) {
            task->frame.regs[REG_ARGUMENT_0] = -err.kind;
        }
        unlockSpinLock(&task->process->lock);
        return true;
    } else {
        unlockSpinLock(&task->process->lock);
        return false;
    }
}

void executeProcessWait(Task* task) {
    lockSpinLock(&process_lock); 
    Error err = basicProcessWait(task);
    unlockSpinLock(&process_lock);
    if (!isError(err)) {
        moveTaskToState(task, ENQUABLE);
    } else {
        if (err.kind == EINTR) {
            lockSpinLock(&task->sched.lock); 
            task->sched.wakeup_function = handleProcessWaitWakeup;
            moveTaskToState(task, SLEEPING);
            unlockSpinLock(&task->sched.lock); 
        } else {
            task->frame.regs[REG_ARGUMENT_0] = -err.kind;
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
        // Reparent children
        Process* child = process->tree.children;
        while (child != NULL) {
            child->tree.parent = process->tree.parent;
            child->tree.child_next = process->tree.parent->tree.children;
            process->tree.parent->tree.children = child;
            child = child->tree.child_next;
        }
        // Remove child from parent
        if (process->tree.child_prev == NULL) {
            process->tree.parent->tree.children = process->tree.child_next;
            if (process->tree.parent->tree.children != NULL) {
                process->tree.parent->tree.children->tree.child_prev = NULL;
            }
        } else {
            process->tree.child_prev->tree.child_next = process->tree.child_next;
            if (process->tree.child_next != NULL) {
                process->tree.child_next->tree.child_prev = process->tree.child_prev;
            }
        }
    } else {
        Process* child = process->tree.children;
        while (child != NULL) {
            child->tree.parent = NULL;
            if (child->tasks == NULL) {
                deallocProcess(child);
            }
            child = child->tree.child_next;
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
        process->tree.parent = parent;
        if (parent != NULL) {
            // Copy session id and process group id
            process->sid = parent->sid;
            process->pgid = parent->pgid;
            // Copy signal handler data, but not pending signals
            memcpy(&process->signals, &parent->signals, sizeof(parent->signals));
            process->signals.signals = NULL;
            process->signals.signals_tail = NULL;
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
            // Set session id and process group id to the pid (probably 1)
            process->sid = process->pid;
            process->pgid = process->pid;
            // Create a new memory space
            process->memory.mem = createMemorySpace();
            process->resources.cwd = stringClone("/");
        }
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

static void moveProcessTaskToState(Task* task, TaskState state) {
    moveTaskToState(task, state);
    sendMessageToAll(YIELD_TASK, task);
}

static void moveAllProcessTasksToStateBut(Process* process, TaskState state, Task* keep) {
    lockSpinLock(&process->lock);
    Task* current = process->tasks;
    while (current != NULL) {
        if (current != keep) {
            moveProcessTaskToState(current, state);
        }
        current = current->proc_next;
    }
    unlockSpinLock(&process->lock);
}

void terminateAllProcessTasksBut(Process* process, Task* keep) {
    moveAllProcessTasksToStateBut(process, TERMINATED, keep);
}

void terminateAllProcessTasks(Process* process) {
    moveAllProcessTasksToStateBut(process, TERMINATED, NULL);
}

void exitProcess(Process* process, Signal signal, int exit) {
    lockSpinLock(&process->lock);
    process->status = (exit & 0xff) | ((signal & 0xff) << 8);
    unlockSpinLock(&process->lock);
    terminateAllProcessTasks(process);
}

void stopProcess(Process* process, Signal signal) {
    lockSpinLock(&process->lock);
    process->status = ((signal & 0xff) << 8) | STATUS_STOPPED << 16;
    if (process->tree.parent != NULL) {
        addSignalToProcess(process->tree.parent, SIGCHLD);
    }
    unlockSpinLock(&process->lock);
    moveAllProcessTasksToStateBut(process, STOPPED, NULL);
}

void continueProcess(Process* process, Signal signal) {
    lockSpinLock(&process->lock);
    process->status = ((signal & 0xff) << 8) | STATUS_RESUMED << 16;
    Task* current = process->tasks;
    while (current != NULL) {
        lockSpinLock(&current->sched.lock);
        if (current->sched.state == STOPPED) {
            current->sched.state = ENQUABLE;
            enqueueTask(current);
        }
        unlockSpinLock(&current->sched.lock);
        current = current->proc_next;
    }
    if (process->tree.parent != NULL) {
        addSignalToProcess(process->tree.parent, SIGCHLD);
    }
    unlockSpinLock(&process->lock);
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
        if (process->tree.parent == NULL) {
            unlockSpinLock(&process->lock);
            deallocProcess(process);
        } else {
            addSignalToProcess(process->tree.parent, SIGCHLD);
            unlockSpinLock(&process->lock);
        }
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

void signalProcessGroup(Pid pgid, Signal signal) {
    lockSpinLock(&process_lock);
    Process* current = global_first;
    while (current != NULL) {
        if (current->pgid == pgid) {
            addSignalToProcess(current, signal);
        }
        current = current->tree.global_next;
    }
    unlockSpinLock(&process_lock);
}

