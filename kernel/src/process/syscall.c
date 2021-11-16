
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
#include "task/harts.h"
#include "task/schedule.h"
#include "process/process.h"
#include "process/syscall.h"
#include "process/signals.h"
#include "process/types.h"
#include "task/types.h"
#include "util/util.h"

typedef struct {
    Task* new_task;
    Task* old_task;
    VfsFile* cur_file;
} ForkSyscallRequest;

static void forkFileDupCallback(Error error, VfsFile* file, void* udata);

static void duplicateFilesStep(ForkSyscallRequest* request) {
    if (request->cur_file == NULL) {
        request->new_task->frame.regs[REG_ARGUMENT_0] = 0;
        request->old_task->frame.regs[REG_ARGUMENT_0] = request->new_task->process->pid;
        request->new_task->sched.state = ENQUABLE;
        enqueueTask(request->new_task);
        request->old_task->sched.state = ENQUABLE;
        enqueueTask(request->old_task);
        dealloc(request);
    } else {
        request->cur_file->functions->dup(request->cur_file, NULL, forkFileDupCallback, request);
    }
}

static void forkFileDupCallback(Error error, VfsFile* file, void* udata) {
    ForkSyscallRequest* request = (ForkSyscallRequest*)udata;
    if (isError(error)) {
        deallocProcess(request->new_task->process);
        deallocTask(request->new_task);
        request->old_task->frame.regs[REG_ARGUMENT_0] = -error.kind;
        enqueueTask(request->old_task);
        dealloc(request);
    } else {
        ProcessResources* new_res = &request->new_task->process->resources;
        file->fd = request->cur_file->fd;
        file->flags = request->cur_file->flags;
        file->next = new_res->files;
        new_res->files = file;
        request->cur_file = request->cur_file->next;
        duplicateFilesStep(request);
    }
}

void forkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    Task* task = (Task*)frame;
    if (frame->hart == NULL || task->process == NULL) {
        // This is for forking kernel processes and trap handlers
        // Will not copy open file descriptors. And memory will remain the same.
        // Only thing that is created is a new process and stack.
        Priority priority = DEFAULT_PRIORITY;
        size_t stack_size = HART_STACK_SIZE;
        void* old_stack_top;
        if (frame->hart == NULL) {
            old_stack_top = ((HartFrame*)frame)->stack_top;
        } else {
            priority = task->sched.priority;
            old_stack_top = (void*)task->stack_top;
        }
        Task* new_task = createKernelTask((void*)frame->pc, stack_size, priority);
        // Copy registers
        memcpy(&new_task->frame, frame, sizeof(TrapFrame));
        // Copy stack
        void* old_stack_pointer = (void*)frame->regs[REG_STACK_POINTER];
        size_t used_size = old_stack_top - old_stack_pointer;
        void* new_stack_pointer = (void*)new_task->stack_top - used_size;
        memcpy(new_stack_pointer, old_stack_pointer, used_size);
        // Update stack pointer and hart value
        new_task->frame.regs[REG_STACK_POINTER] = (uintptr_t)new_stack_pointer;
        if (frame->hart == NULL && new_task->frame.hart == NULL) {
            new_task->frame.hart = (HartFrame*)frame;
        }
        // Set the return value of the syscall to 1 for the new process
        new_task->frame.regs[REG_ARGUMENT_0] = 1;
        frame->regs[REG_ARGUMENT_0] = 0;
        enqueueTask(new_task);
    } else {
        Process* new_process = createUserProcess(task->process);
        Task* new_task = createTaskInProcess(
            new_process, task->frame.regs[REG_STACK_POINTER], task->frame.regs[REG_GLOBAL_POINTER],
            task->frame.pc, task->sched.priority
        );
        if (new_process != NULL && new_process->memory.mem != NULL) {
            // Copy frame
            new_task->frame.pc = task->frame.pc;
            memcpy(&new_task->frame.regs, &task->frame.regs, sizeof(task->frame.regs));
            memcpy(&new_task->frame.fregs, &task->frame.fregs, sizeof(task->frame.fregs));
            // Copy signal handler data, but not pending signals
            new_process->signals.current_signal = task->process->signals.current_signal;
            new_process->signals.restore_frame = task->process->signals.restore_frame;
            memcpy(
                new_process->signals.handlers, task->process->signals.handlers,
                sizeof(task->process->signals.handlers)
            );
            // Copy files
            new_process->resources.uid = task->process->resources.uid;
            new_process->resources.gid = task->process->resources.gid;
            new_process->resources.next_fd = task->process->resources.next_fd;
            new_process->memory.start_brk = task->process->memory.start_brk;
            new_process->memory.brk = task->process->memory.brk;
            if (task->process->resources.files != NULL) {
                task->sched.state = WAITING;
                new_task->sched.state = WAITING;
                ForkSyscallRequest* request = kalloc(sizeof(ForkSyscallRequest));
                request->new_task = new_task;
                request->old_task = task;
                request->cur_file = task->process->resources.files;
                duplicateFilesStep(request);
            } else {
                task->sched.state = ENQUABLE;
                new_task->sched.state = ENQUABLE;
                // No files have to be duplicated
                new_task->frame.regs[REG_ARGUMENT_0] = 0;
                task->frame.regs[REG_ARGUMENT_0] = new_process->pid;
                enqueueTask(new_task);
            }
        } else {
            deallocTask(new_task);
            deallocProcess(new_process);
            task->frame.regs[REG_ARGUMENT_0] = -ENOMEM;
        }
    }
}

void exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    task->process->status = (task->process->status & ~0xff) | (args[0] & 0xff);
    terminateAllProcessTasks(task->process);
    deallocProcess(task->process);
}

void pauseSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    task->sched.state = PAUSED;
}

void alarmSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process->signals.alarm_at == 0) {
        task->frame.regs[REG_ARGUMENT_0] = 0;
    } else {
        Time time = getTime();
        if (time >= task->process->signals.alarm_at) {
            task->frame.regs[REG_ARGUMENT_0] = 1;
        } else {
            task->frame.regs[REG_ARGUMENT_0] = umax(1, (task->process->signals.alarm_at - time) / CLOCKS_PER_SEC);
        }
    }
    if (args[0] == 0) {
        task->process->signals.alarm_at = 0;
    } else {
        Time delay = args[0] * CLOCKS_PER_SEC; // Argument is in seconds
        Time end = getTime() + delay;
        task->process->signals.alarm_at = end;
    }
}

void getpidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    task->frame.regs[REG_ARGUMENT_0] = task->process->pid;
}

void getppidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process->tree.parent != NULL) {
        task->frame.regs[REG_ARGUMENT_0] = task->process->tree.parent->pid;
    } else {
        task->frame.regs[REG_ARGUMENT_0] = 0;
    }
}

void waitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    executeProcessWait(task);
}

typedef SignalHandler SignalAction;

void sigactionSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    Signal sig = args[0];
    if (sig >= SIG_COUNT || sig == 0) {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
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
        VirtPtr new = virtPtrForTask(args[1], task);
        VirtPtr old = virtPtrForTask(args[2], task);
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
        task->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    }
}

void sigreturnSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    // If we are not in a handler, this is an error
    task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    returnFromSignal(task);
}

void sigpendingSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    SignalSet set = 0;
    PendingSignal* current = task->process->signals.signals;
    while (current != NULL) {
        set |= (1UL << (current->signal - 1));
        current = current->next;
    }
    task->frame.regs[REG_ARGUMENT_0] = set & task->process->signals.mask;
}

typedef enum {
    SIG_SETMASK = 0,
    SIG_BLOCK,
    SIG_UNBLOCK,
} SigProcHow;

void sigprocmaskSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    SignalSet old = task->process->signals.mask;
    SigProcHow how = args[0];
    SignalSet new = args[1];
    if (how == SIG_SETMASK) {
        task->process->signals.mask = new;
    } else if (how == SIG_BLOCK) {
        task->process->signals.mask |= new;
    } else if (how == SIG_UNBLOCK) {
        task->process->signals.mask &= ~new;
    }
    task->frame.regs[REG_ARGUMENT_0] = old;
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

void killSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    int pid = args[0];
    task->frame.regs[REG_ARGUMENT_0] = doForProcessWithPid(pid, killSyscallCallback, task);
}

void setUidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process->resources.uid == 0 || task->process->resources.gid == 0) {
        task->process->resources.uid = args[0];
        task->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EACCES;
    }
}

void setGidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process->resources.uid == 0 || task->process->resources.gid == 0) {
        task->process->resources.gid = args[0];
        task->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EACCES;
    }
}

void getUidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    task->frame.regs[REG_ARGUMENT_0] = task->process->resources.uid;
}

void getGidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    task->frame.regs[REG_ARGUMENT_0] = task->process->resources.gid;
}

void exit() {
    syscall(SYSCALL_EXIT);
    panic(); // This will never return
}

