
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "interrupt/syscall.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "process/harts.h"
#include "process/process.h"
#include "process/syscall.h"
#include "process/schedule.h"
#include "process/types.h"
#include "util/util.h"

uintptr_t forkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    Process* process = (Process*)frame;
    if (frame->hart == NULL || process->pid == 0) {
        // This is for forking kernel processes and trap handlers
        Priority priority = DEFAULT_PRIORITY;
        size_t stack_size = HART_STACK_SIZE;
        void* old_stack_top;
        if (frame->hart == NULL) {
            old_stack_top = ((HartFrame*)frame)->stack_top;
        } else {
            priority = process->priority;
            stack_size = kallocSize(process->stack);
            old_stack_top = process->stack + stack_size;
        }
        Process* new_process = createKernelProcess((void*)frame->pc, priority, stack_size);
        // Copy registers
        memcpy(&new_process->frame, frame, sizeof(TrapFrame));
        // Copy stack
        void* old_stack_pointer = (void*)frame->regs[REG_STACK_POINTER];
        size_t used_size = old_stack_top - old_stack_pointer;
        void* new_stack_pointer = new_process->stack + stack_size - used_size;
        memcpy(new_stack_pointer, old_stack_pointer, used_size);
        // Update stack pointer and hart value
        new_process->frame.regs[REG_STACK_POINTER] = (uintptr_t)new_stack_pointer;
        if (frame->hart == NULL && new_process->frame.hart == NULL) {
            new_process->frame.hart = (HartFrame*)frame;
        }
        // Set the return value of the syscall to 1 for the new process
        new_process->frame.regs[REG_ARGUMENT_0] = 1;
        enqueueProcess(new_process);
    } else {
        // TODO: Implement forks of user processes
    }
    return 0;
}

uintptr_t exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->state = args[0];
    process->state = TERMINATED;
    return 0;
}

uintptr_t yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    // TODO: Improve the scheduler completely
    assert(frame->hart != NULL); // Only a process can be yielded
    Process* process = (Process*)frame;
    // Decrease priority to allow other processes to run
    // All other processes will be run at least once before running this one again
    process->sched_priority = LOWEST_PRIORITY;
    return 0;
}

void exit() {
    syscall(SYSCALL_EXIT);
    panic(); // This will never return
}

