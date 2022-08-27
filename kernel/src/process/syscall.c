
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "files/vfs.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "memory/kalloc.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"
#include "process/process.h"
#include "process/signals.h"
#include "process/syscall.h"
#include "process/types.h"
#include "task/harts.h"
#include "task/schedule.h"
#include "task/types.h"
#include "util/util.h"

SyscallReturn forkSyscall(TrapFrame* frame) {
    Task* task = (Task*)frame;
    if (frame->hart == NULL || task->process == NULL) {
        // This is for forking kernel processes and trap handlers
        // Will not copy open file descriptors. And memory will remain the same.
        // Only thing that is created is a new task and stack.
        Priority priority = DEFAULT_PRIORITY;
        size_t stack_size = HART_STACK_SIZE;
        uintptr_t old_stack_top;
        if (frame->hart == NULL) {
            old_stack_top = ((HartFrame*)frame)->stack_top;
        } else {
            priority = task->sched.priority;
            old_stack_top = task->stack_top;
        }
        Task* new_task = createKernelTask((void*)frame->pc, stack_size, priority);
        // Copy registers
        memcpy(&new_task->frame, frame, sizeof(TrapFrame));
        // Copy stack
        uintptr_t old_stack_pointer = frame->regs[REG_STACK_POINTER];
        size_t used_size = old_stack_top - old_stack_pointer;
        void* new_stack_pointer = (void*)new_task->stack_top - used_size;
        memcpy(new_stack_pointer, (void*)old_stack_pointer, used_size);
        // Update stack pointer and hart value
        new_task->frame.regs[REG_STACK_POINTER] = (uintptr_t)new_stack_pointer;
        if (frame->hart == NULL && new_task->frame.hart == NULL) {
            new_task->frame.hart = (HartFrame*)frame;
        }
        // Set the return value of the syscall to 1 for the new process
        new_task->frame.regs[REG_ARGUMENT_0] = 1;
        enqueueTask(new_task);
        SYSCALL_RETURN(0);
    } else {
        Process* new_process = createUserProcess(task->process);
        Task* new_task = NULL;
        if (new_process != NULL && new_process->memory.mem != NULL) {
            new_task = createTaskInProcess(
                new_process, task->frame.regs[REG_STACK_POINTER], task->frame.regs[REG_GLOBAL_POINTER],
                task->frame.pc, task->sched.priority
            );
        }
        if (new_task != NULL) {
            // Copy frame
            new_task->frame.pc = task->frame.pc;
            memcpy(&new_task->frame.regs, &task->frame.regs, sizeof(task->frame.regs));
            memcpy(&new_task->frame.fregs, &task->frame.fregs, sizeof(task->frame.fregs));
            new_task->frame.regs[REG_ARGUMENT_0] = 0;
            enqueueTask(new_task);
            SYSCALL_RETURN(new_process->pid);
        } else {
            deallocProcess(new_process);
            SYSCALL_RETURN(-ENOMEM);
        }
    }
}

SyscallReturn exitSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process != NULL) {
        exitProcess(task->process, 0, SYSCALL_ARG(0));
    } else {
        moveTaskToState(task, TERMINATED);
    }
    enqueueTask(task);
    return WAIT;
}

SyscallReturn pauseSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    moveTaskToState(task, PAUSED);
    enqueueTask(task);
    return WAIT;
}

SyscallReturn alarmSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    uintptr_t curr_time = 0;
    if (task->process->signals.alarm_at != 0) {
        Time time = getTime();
        if (time >= task->process->signals.alarm_at) {
            time = 1;
        } else {
            time = umax(1, (task->process->signals.alarm_at - time) / CLOCKS_PER_SEC);
        }
    }
    if (SYSCALL_ARG(0) == 0) {
        task->process->signals.alarm_at = 0;
    } else {
        Time delay = SYSCALL_ARG(0) * CLOCKS_PER_SEC; // Argument is in seconds
        Time end = getTime() + delay;
        task->process->signals.alarm_at = end;
    }
    SYSCALL_RETURN(curr_time);
}

SyscallReturn getpidSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    SYSCALL_RETURN(task->process->pid);
}

SyscallReturn getppidSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    if (task->process->tree.parent != NULL) {
        SYSCALL_RETURN(task->process->tree.parent->pid);
    } else {
        SYSCALL_RETURN(0);
    }
}

SyscallReturn waitSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    executeProcessWait(task);
    enqueueTask(task);
    return WAIT;
}

typedef SignalHandler SignalAction;

SyscallReturn sigactionSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    Signal sig = SYSCALL_ARG(0);
    if (sig >= SIG_COUNT || sig == 0) {
        SYSCALL_RETURN(-EINVAL);
    } else {
        lockSpinLock(&task->process->lock);
        SignalAction oldaction = {
            .handler = 0,
            .mask = 0,
            .flags = 0,
            .sigaction = 0,
            .restorer = 0,
        };
        if (task->process->signals.handlers[sig] != SIG_DFL && task->process->signals.handlers[sig] != SIG_IGN) {
            oldaction = *task->process->signals.handlers[sig];
        }
        SignalAction newaction;
        VirtPtr new = virtPtrForTask(SYSCALL_ARG(1), task);
        VirtPtr old = virtPtrForTask(SYSCALL_ARG(2), task);
        if (new.address != 0) {
            memcpyBetweenVirtPtr(virtPtrForKernel(&newaction), new, sizeof(SignalAction));
            if (newaction.handler == (uintptr_t)SIG_DFL || newaction.handler == (uintptr_t)SIG_IGN) {
                if (task->process->signals.handlers[sig] != SIG_DFL && task->process->signals.handlers[sig] != SIG_IGN) {
                    dealloc(task->process->signals.handlers[sig]);
                }
                task->process->signals.handlers[sig] = (SignalHandler*)newaction.handler;
            } else {
                if (task->process->signals.handlers[sig] == SIG_DFL || task->process->signals.handlers[sig] == SIG_IGN) {
                    task->process->signals.handlers[sig] = kalloc(sizeof(SignalHandler));
                }
                *task->process->signals.handlers[sig] = newaction;
            }
        }
        unlockSpinLock(&task->process->lock);
        if (old.address != 0) {
            memcpyBetweenVirtPtr(old, virtPtrForKernel(&oldaction), sizeof(SignalAction));
        }
        SYSCALL_RETURN(-SUCCESS);
    }
}

SyscallReturn sigreturnSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    // If we are not in a handler, this is an error
    task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    returnFromSignal(task);
    return CONTINUE;
}

SyscallReturn sigpendingSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    SignalSet set = 0;
    PendingSignal* current = task->process->signals.signals;
    while (current != NULL) {
        set |= (1UL << (current->signal - 1));
        current = current->next;
    }
    SYSCALL_RETURN(set & task->process->signals.mask);
}

typedef enum {
    SIG_SETMASK = 0,
    SIG_BLOCK,
    SIG_UNBLOCK,
} SigProcHow;

SyscallReturn sigprocmaskSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    SignalSet old = task->process->signals.mask;
    SigProcHow how = SYSCALL_ARG(0);
    SignalSet new = SYSCALL_ARG(1);
    if (how == SIG_SETMASK) {
        task->process->signals.mask = new;
    } else if (how == SIG_BLOCK) {
        task->process->signals.mask |= new;
    } else if (how == SIG_UNBLOCK) {
        task->process->signals.mask &= ~new;
    }
    SYSCALL_RETURN(old);
}

int killSyscallCallback(Process* process, void* udata) {
    Task* task = (Task*)udata;
    if (task->process->resources.uid == 0 || process->resources.uid == task->process->resources.uid) {
        addSignalToProcess(process, task->frame.regs[REG_ARGUMENT_2]);
        return -SUCCESS;
    } else {
        return -EACCES;
    }
}

SyscallReturn killSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    int pid = SYSCALL_ARG(0);
    SYSCALL_RETURN(doForProcessWithPid(pid, killSyscallCallback, task));
}

SyscallReturn setUidSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    if (task->process->resources.uid == 0 || task->process->resources.gid == 0) {
        task->process->resources.uid = SYSCALL_ARG(0);
        SYSCALL_RETURN(-SUCCESS);
    } else {
        SYSCALL_RETURN(-EACCES);
    }
}

SyscallReturn setGidSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    if (task->process->resources.uid == 0 || task->process->resources.gid == 0) {
        task->process->resources.gid = SYSCALL_ARG(0);
        SYSCALL_RETURN(-SUCCESS);
    } else {
        SYSCALL_RETURN(-EACCES);
    }
}

SyscallReturn getUidSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    SYSCALL_RETURN(task->process->resources.uid);
}

SyscallReturn getGidSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    SYSCALL_RETURN(task->process->resources.gid);
}

noreturn void leave() {
    syscall(SYSCALL_EXIT);
    panic(); // This will never return
}

