
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
} ForkSyscallRequest;

static void forkFileDupCallback(Error error, VfsFile* file, void* udata) {
    ForkSyscallRequest* request = (ForkSyscallRequest*)udata;
    if (isError(error)) {
        deallocProcess(request->new_process);
        request->old_process->frame.regs[REG_ARGUMENT_0] = -error.kind;
        enqueueProcess(request->old_process);
        dealloc(request);
    } else {
        ProcessResources* old_res = &request->old_process->resources;
        ProcessResources* new_res = &request->new_process->resources;
        new_res->filedes[new_res->fd_count] = file;
        new_res->filedes[new_res->fd_count]->fd = old_res->filedes[new_res->fd_count]->fd;
        new_res->filedes[new_res->fd_count]->flags = old_res->filedes[new_res->fd_count]->flags;
        new_res->fd_count++;
        if (new_res->fd_count < old_res->fd_count) {
            VfsFile* old_file = old_res->filedes[new_res->fd_count];
            old_file->functions->dup(old_file, 0, 0, forkFileDupCallback, request);
        } else {
            request->new_process->frame.regs[REG_ARGUMENT_0] = 0;
            request->old_process->frame.regs[REG_ARGUMENT_0] = request->new_process->pid;
            moveToSchedState(request->new_process, ENQUEUEABLE);
            enqueueProcess(request->new_process);
            moveToSchedState(request->old_process, ENQUEUEABLE);
            enqueueProcess(request->old_process);
            dealloc(request);
        }
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
        if (new_process != NULL && copyAllPagesAndAllocUsers(new_process->memory.table, process->memory.table)) {
            // Copy frame
            new_process->frame.pc = process->frame.pc;
            memcpy(&new_process->frame.regs, &process->frame.regs, sizeof(process->frame.regs));
            memcpy(&new_process->frame.fregs, &process->frame.fregs, sizeof(process->frame.fregs));
            // Copy signal handler data, but not pending signals
            new_process->signals.current_signal = process->signals.current_signal;
            new_process->signals.restore_frame = process->signals.restore_frame;
            memcpy(new_process->signals.handlers, process->signals.handlers, sizeof(process->signals.handlers));
            // Copy files
            size_t fd_count = process->resources.fd_count;
            new_process->resources.uid = process->resources.uid;
            new_process->resources.gid = process->resources.gid;
            new_process->resources.next_fd = process->resources.next_fd;
            new_process->resources.fd_count = 0;
            new_process->memory.start_brk = process->memory.start_brk;
            new_process->memory.brk = process->memory.brk;
            if (fd_count != 0) {
                moveToSchedState(process, WAITING);
                moveToSchedState(new_process, WAITING);
                new_process->resources.filedes = kalloc(fd_count * sizeof(ProcessFileDescEntry));
                ForkSyscallRequest* request = kalloc(sizeof(ForkSyscallRequest));
                request->new_process = new_process;
                request->old_process = process;
                process->resources.filedes[0]->functions->dup(
                    process->resources.filedes[0], 0, 0, forkFileDupCallback, request
                );
            } else {
                // No files have to be duplicated
                new_process->frame.regs[REG_ARGUMENT_0] = 0;
                process->frame.regs[REG_ARGUMENT_0] = new_process->pid;
                enqueueProcess(new_process);
            }
        } else {
            deallocProcess(new_process);
            process->frame.regs[REG_ARGUMENT_0] = -ALREADY_IN_USE;
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

typedef struct {
    uintptr_t handler;
    uintptr_t restorer;
} SignalAction;

void sigactionSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    Signal sig = args[0];
    if (sig >= SIG_COUNT || sig == 0) {
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
    } else {
        lockSpinLock(&process->signals.lock);
        SignalAction sigaction = {
            .handler = process->signals.handlers[sig].handler,
            .restorer = process->signals.handlers[sig].restorer,
        };
        VirtPtr new = virtPtrFor(args[1], process->memory.table);
        VirtPtr old = virtPtrFor(args[2], process->memory.table);
        if (old.address != 0) {
            memcpyBetweenVirtPtr(old, virtPtrForKernel(&sigaction), sizeof(SignalAction));
        }
        if (new.address != 0) {
            memcpyBetweenVirtPtr(virtPtrForKernel(&sigaction), new, sizeof(SignalAction));
            process->signals.handlers[sig].handler = sigaction.handler;
            process->signals.handlers[sig].restorer = sigaction.restorer;
        }
        unlockSpinLock(&process->signals.lock);
        process->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    }
}

void sigreturnSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    // If we are not in a handler, this is an error
    process->frame.regs[REG_ARGUMENT_0] = -UNSUPPORTED;
    returnFromSignal(process);
}

int killSyscallCallback(Process* process, void* udata) {
    Process* proc = (Process*)udata;
    if (proc->resources.uid == 0 || process->resources.uid == proc->resources.uid) {
        addSignalToProcess(process, proc->frame.regs[REG_ARGUMENT_2]);
        return 0;
    } else {
        return -FORBIDDEN;
    }
}

void killSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    int pid = args[0];
    process->frame.regs[REG_ARGUMENT_0] = doForProcessWithPid(pid, killSyscallCallback, process);
}

void exit() {
    syscall(SYSCALL_EXIT);
    panic(); // This will never return
}

