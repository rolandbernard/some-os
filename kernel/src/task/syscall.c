
#include <assert.h>

#include "task/syscall.h"

#include "task/harts.h"

SyscallReturn yieldSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL); // Only a tasks can be yielded
    // Do nothing, task will be enqueued
    SYSCALL_RETURN(0);
}

SyscallReturn sleepSyscall(TrapFrame* frame) {
    Time delay = SYSCALL_ARG(0) / (1000000000UL / CLOCKS_PER_SEC); // Argument is in nanoseconds
    Time end = getTime() + delay;
    if (frame->hart == NULL) {
        // If this is not a task. Just loop.
        while (end > getTime()) {
            // Wait for the time to come
        }
        SYSCALL_RETURN(0);
    } else {
        // This is a task. We can put it into wait.
        Task* task = (Task*)frame;
        task->sched.sleeping_until = end;
        task->sched.state = SLEEPING;
        return WAIT;
    }
}

SyscallReturn criticalSyscall(TrapFrame* frame) {
    if (frame->hart != NULL) {
        frame->regs[REG_ARGUMENT_0] = (uintptr_t)frame;
        loadTrapFrame(frame, getCurrentTrapFrame());
    } else {
        frame->regs[REG_ARGUMENT_0] = 0;
    }
    return CONTINUE;
}

TrapFrame* criticalEnter() {
    if (getCurrentTask() != NULL) {
        return (TrapFrame*)syscall(SYSCALL_CRITICAL);
    } else {
        return NULL;
    }
}

void criticalReturn(TrapFrame* to) {
    if (to != NULL && getCurrentTask() == NULL) {
        loadTrapFrame(getCurrentTrapFrame(), to);
    }
}

