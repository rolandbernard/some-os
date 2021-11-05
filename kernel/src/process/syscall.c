
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
#include "process/harts.h"
#include "process/process.h"
#include "process/syscall.h"
#include "process/schedule.h"
#include "process/signals.h"
#include "process/types.h"
#include "util/util.h"

typedef struct {
    Process* new_process;
    Process* old_process;
    VfsFile* cur_file;
} ForkSyscallRequest;

static void forkFileDupCallback(Error error, VfsFile* file, void* udata);

static void duplicateFilesStep(ForkSyscallRequest* request) {
    if (request->cur_file == NULL) {
        request->new_process->frame.regs[REG_ARGUMENT_0] = 0;
        request->old_process->frame.regs[REG_ARGUMENT_0] = request->new_process->pid;
        moveToSchedState(request->new_process, ENQUEUEABLE);
        enqueueProcess(request->new_process);
        moveToSchedState(request->old_process, ENQUEUEABLE);
        enqueueProcess(request->old_process);
        dealloc(request);
    } else {
        request->cur_file->functions->dup(request->cur_file, NULL, forkFileDupCallback, request);
    }
}

static void forkFileDupCallback(Error error, VfsFile* file, void* udata) {
    ForkSyscallRequest* request = (ForkSyscallRequest*)udata;
    if (isError(error)) {
        deallocProcess(request->new_process);
        request->old_process->frame.regs[REG_ARGUMENT_0] = -error.kind;
        enqueueProcess(request->old_process);
        dealloc(request);
    } else {
        ProcessResources* new_res = &request->new_process->resources;
        file->fd = request->cur_file->fd;
        file->flags = request->cur_file->flags;
        file->next = new_res->files;
        new_res->files = file;
        request->cur_file = request->cur_file->next;
        duplicateFilesStep(request);
    }
}

void forkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    Process* process = (Process*)frame;
    if (frame->hart == NULL || process->pid == 0) {
        // This is for forking kernel processes and trap handlers
        // Will not copy open file descriptors. And memory will remain the same.
        // Only thing that is created is a new process and stack.
        Priority priority = DEFAULT_PRIORITY;
        size_t stack_size = HART_STACK_SIZE;
        void* old_stack_top;
        if (frame->hart == NULL) {
            old_stack_top = ((HartFrame*)frame)->stack_top;
        } else {
            priority = process->sched.priority;
            stack_size = kallocSize(process->memory.stack);
            old_stack_top = process->memory.stack + stack_size;
        }
        Process* new_process = createKernelProcess((void*)frame->pc, priority, stack_size);
        // Copy registers
        memcpy(&new_process->frame, frame, sizeof(TrapFrame));
        // Copy stack
        void* old_stack_pointer = (void*)frame->regs[REG_STACK_POINTER];
        size_t used_size = old_stack_top - old_stack_pointer;
        void* new_stack_pointer = new_process->memory.stack + stack_size - used_size;
        memcpy(new_stack_pointer, old_stack_pointer, used_size);
        // Update stack pointer and hart value
        new_process->frame.regs[REG_STACK_POINTER] = (uintptr_t)new_stack_pointer;
        if (frame->hart == NULL && new_process->frame.hart == NULL) {
            new_process->frame.hart = (HartFrame*)frame;
        }
        // Set the return value of the syscall to 1 for the new process
        new_process->frame.regs[REG_ARGUMENT_0] = 1;
        frame->regs[REG_ARGUMENT_0] = 0;
        enqueueProcess(new_process);
    } else {
        Process* new_process = createChildUserProcess(process);
        if (new_process != NULL && new_process->memory.mem != NULL) {
            // Copy frame
            new_process->frame.pc = process->frame.pc;
            memcpy(&new_process->frame.regs, &process->frame.regs, sizeof(process->frame.regs));
            memcpy(&new_process->frame.fregs, &process->frame.fregs, sizeof(process->frame.fregs));
            // Copy signal handler data, but not pending signals
            new_process->signals.current_signal = process->signals.current_signal;
            new_process->signals.restore_frame = process->signals.restore_frame;
            memcpy(new_process->signals.handlers, process->signals.handlers, sizeof(process->signals.handlers));
            // Copy files
            new_process->resources.uid = process->resources.uid;
            new_process->resources.gid = process->resources.gid;
            new_process->resources.next_fd = process->resources.next_fd;
            new_process->memory.start_brk = process->memory.start_brk;
            new_process->memory.brk = process->memory.brk;
            if (process->resources.files != NULL) {
                moveToSchedState(process, WAITING);
                moveToSchedState(new_process, WAITING);
                ForkSyscallRequest* request = kalloc(sizeof(ForkSyscallRequest));
                request->new_process = new_process;
                request->old_process = process;
                request->cur_file = process->resources.files;
                duplicateFilesStep(request);
            } else {
                // No files have to be duplicated
                new_process->frame.regs[REG_ARGUMENT_0] = 0;
                process->frame.regs[REG_ARGUMENT_0] = new_process->pid;
                enqueueProcess(new_process);
            }
        } else {
            deallocProcess(new_process);
            process->frame.regs[REG_ARGUMENT_0] = -ENOMEM;
        }
    }
}

void exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->status = (process->status & ~0xff) | (args[0] & 0xff);
    moveToSchedState(process, TERMINATED);
}

void yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL); // Only a process can be yielded
    // Do nothing, process will be enqueued
}

void sleepSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    Time delay = args[0] / (1000000000UL / CLOCKS_PER_SEC); // Argument is in nanoseconds
    Time end = getTime() + delay;
    if (frame->hart == NULL) {
        // If this is not a process. Just loop.
        while (end > getTime()) {
            // Wait for the time to come
        }
    } else {
        // This is a process. We can put it into wait.
        Process* process = (Process*)frame;
        process->sched.sleeping_until = end;
        moveToSchedState(process, SLEEPING);
    }
}

void pauseSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    moveToSchedState(process, PAUSED);
}

void alarmSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    if (process->signals.alarm_at == 0) {
        process->frame.regs[REG_ARGUMENT_0] = 0;
    } else {
        Time time = getTime();
        if (time >= process->signals.alarm_at) {
            process->frame.regs[REG_ARGUMENT_0] = 1;
        } else {
            process->frame.regs[REG_ARGUMENT_0] = umax(1, (process->signals.alarm_at - time) / CLOCKS_PER_SEC);
        }
    }
    if (args[0] == 0) {
        process->signals.alarm_at = 0;
    } else {
        Time delay = args[0] * CLOCKS_PER_SEC; // Argument is in seconds
        Time end = getTime() + delay;
        process->signals.alarm_at = end;
    }
}

void getpidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->frame.regs[REG_ARGUMENT_0] = process->pid;
}

void getppidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    if (process->tree.parent != NULL) {
        process->frame.regs[REG_ARGUMENT_0] = process->tree.parent->pid;
    } else {
        process->frame.regs[REG_ARGUMENT_0] = 0;
    }
}

void waitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    executeProcessWait(process);
}

typedef SignalHandler SignalAction;

void sigactionSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    Signal sig = args[0];
    if (sig >= SIG_COUNT || sig == 0) {
        process->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    } else {
        lockSpinLock(&process->signals.lock);
        SignalAction oldaction = {
            .handler = 0,
            .mask = 0,
            .flags = 0,
            .sigaction = 0,
            .restorer = 0,
        };
        if (process->signals.handlers[sig] != SIG_DFL && process->signals.handlers[sig] != SIG_IGN) {
            oldaction = *process->signals.handlers[sig];
        }
        SignalAction newaction;
        VirtPtr new = virtPtrFor(args[1], process->memory.mem);
        VirtPtr old = virtPtrFor(args[2], process->memory.mem);
        if (new.address != 0) {
            memcpyBetweenVirtPtr(virtPtrForKernel(&newaction), new, sizeof(SignalAction));
            if (newaction.handler == (uintptr_t)SIG_DFL || newaction.handler == (uintptr_t)SIG_IGN) {
                if (process->signals.handlers[sig] != SIG_DFL && process->signals.handlers[sig] != SIG_IGN) {
                    dealloc(process->signals.handlers[sig]);
                }
                process->signals.handlers[sig] = (SignalHandler*)newaction.handler;
            } else {
                if (process->signals.handlers[sig] == SIG_DFL || process->signals.handlers[sig] == SIG_IGN) {
                    process->signals.handlers[sig] = kalloc(sizeof(SignalHandler));
                }
                *process->signals.handlers[sig] = newaction;
            }
        }
        unlockSpinLock(&process->signals.lock);
        if (old.address != 0) {
            memcpyBetweenVirtPtr(old, virtPtrForKernel(&oldaction), sizeof(SignalAction));
        }
        process->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    }
}

void sigreturnSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    // If we are not in a handler, this is an error
    process->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    returnFromSignal(process);
}

void sigpendingSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    SignalSet set = 0;
    PendingSignal* current = process->signals.signals;
    while (current != NULL) {
        set |= (1UL << (current->signal - 1));
        current = current->next;
    }
    process->frame.regs[REG_ARGUMENT_0] = set & process->signals.mask;
}

typedef enum {
    SIG_SETMASK = 0,
    SIG_BLOCK,
    SIG_UNBLOCK,
} SigProcHow;

void sigprocmaskSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    SignalSet old = process->signals.mask;
    SigProcHow how = args[0];
    SignalSet new = args[1];
    if (how == SIG_SETMASK) {
        process->signals.mask = new;
    } else if (how == SIG_BLOCK) {
        process->signals.mask |= new;
    } else if (how == SIG_UNBLOCK) {
        process->signals.mask &= ~new;
    }
    process->frame.regs[REG_ARGUMENT_0] = old;
}

int killSyscallCallback(Process* process, void* udata) {
    Process* proc = (Process*)udata;
    if (proc->resources.uid == 0 || process->resources.uid == proc->resources.uid) {
        addSignalToProcess(process, proc->frame.regs[REG_ARGUMENT_2]);
        return -SUCCESS;
    } else {
        return -EACCES;
    }
}

void killSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    int pid = args[0];
    process->frame.regs[REG_ARGUMENT_0] = doForProcessWithPid(pid, killSyscallCallback, process);
}

void setUidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    if (process->resources.uid == 0 || process->resources.gid == 0) {
        process->resources.uid = args[0];
        process->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -EACCES;
    }
}

void setGidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    if (process->resources.uid == 0 || process->resources.gid == 0) {
        process->resources.gid = args[0];
        process->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -EACCES;
    }
}

void getUidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->frame.regs[REG_ARGUMENT_0] = process->resources.uid;
}

void getGidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->frame.regs[REG_ARGUMENT_0] = process->resources.gid;
}

void exit() {
    syscall(SYSCALL_EXIT);
    panic(); // This will never return
}

