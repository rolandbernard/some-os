
#include <assert.h>

#include "task/syscall.h"

void yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL); // Only a tasks can be yielded
    // Do nothing, task will be enqueued
}

void sleepSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    Time delay = args[0] / (1000000000UL / CLOCKS_PER_SEC); // Argument is in nanoseconds
    Time end = getTime() + delay;
    if (frame->hart == NULL) {
        // If this is not a task. Just loop.
        while (end > getTime()) {
            // Wait for the time to come
        }
    } else {
        // This is a task. We can put it into wait.
        Task* task = (Task*)frame;
        task->sched.sleeping_until = end;
        task->sched.state = SLEEPING;
    }
}

void criticalSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    if (is_kernel) {
        frame->regs[REG_ARGUMENT_0] = (uintptr_t)task;
        // TODO: enter the frame in non-preemptable context
    } else {
        frame->regs[REG_ARGUMENT_0] = -EPERM;
    }
}

