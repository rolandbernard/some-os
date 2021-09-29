
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "files/vfs.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "process/harts.h"
#include "process/process.h"
#include "process/syscall.h"
#include "process/schedule.h"
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
        new_res->fds[new_res->fd_count] = old_res->fds[new_res->fd_count];
        new_res->files[new_res->fd_count] = file;
        new_res->fd_count++;
        if (new_res->fd_count < old_res->fd_count) {
            VfsFile* old_file = old_res->files[new_res->fd_count];
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
        if (copyAllPagesAndAllocUsers(new_process->memory.table, process->memory.table)) {
            uintptr_t sp = new_process->frame.regs[REG_STACK_POINTER];
            KERNEL_LOG("%p -> %p %p", sp, virtToPhys(process->memory.table, sp), process->frame.satp);
            KERNEL_LOG("%p -> %p %p", sp, virtToPhys(new_process->memory.table, sp), new_process->frame.satp);
            size_t fd_count = process->resources.fd_count;
            new_process->resources.uid = process->resources.uid;
            new_process->resources.gid = process->resources.gid;
            new_process->resources.next_fd = process->resources.next_fd;
            new_process->resources.fd_count = 0;
            if (fd_count != 0) {
                moveToSchedState(process, WAITING);
                moveToSchedState(new_process, WAITING);
                new_process->resources.files = kalloc(fd_count * sizeof(VfsFile*));
                new_process->resources.fds = kalloc(fd_count * sizeof(int));
                ForkSyscallRequest* request = kalloc(sizeof(ForkSyscallRequest));
                request->new_process = new_process;
                request->old_process = process;
                process->resources.files[0]->functions->dup(
                    process->resources.files[0], 0, 0, forkFileDupCallback, request
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

// TODO: implement sleep differently
static void awakenFromSleep(Time time, void* udata) {
    Process* process = (Process*)udata;
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void sleepSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    Time delay = args[0] * CLOCKS_PER_SEC / 1000000000UL; // Argument is in nanoseconds
    if (frame->hart == NULL) {
        // If this is not a process. Just loop.
        Time end = getTime() + delay;
        while (end > getTime()) {
            // Wait for the time to come
        }
    } else {
        // This is a process. We can put it into wait.
        Process* process = (Process*)frame;
        moveToSchedState(process, SLEEPING);
        setTimeout(delay, awakenFromSleep, process);
    }
}

void exit() {
    syscall(SYSCALL_EXIT);
    panic(); // This will never return
}

