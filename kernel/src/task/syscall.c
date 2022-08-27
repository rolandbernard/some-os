
#include <assert.h>

#include "task/syscall.h"

#include "task/harts.h"
#include "task/schedule.h"

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
        moveTaskToState(task, SLEEPING);
        enqueueTask(task);
        return WAIT;
    }
}

SyscallReturn criticalSyscall(TrapFrame* frame) {
#ifdef CRITICAL_FAST_PATH
    // This is handled by a special fast path in the trap handler
    panic();
#else
    if (frame->hart != NULL) {
        frame->regs[REG_ARGUMENT_0] = (uintptr_t)frame;
        swapTrapFrame(frame, getCurrentTrapFrame());
    } else {
        frame->regs[REG_ARGUMENT_0] = 0;
    }
    return CONTINUE;
#endif
}

Task* criticalEnter() {
    if (getCurrentTask() != NULL) {
        return (Task*)syscall(SYSCALL_CRITICAL);
    } else {
        return NULL;
    }
}

extern void criticalReturnFastPath(Task* to);

void criticalReturn(Task* to) {
    assert(getCurrentTask() == NULL);
    if (to != NULL) {
#ifdef CRITICAL_FAST_PATH
        criticalReturnFastPath(to);
#else
        swapTrapFrame(getCurrentTrapFrame(), &to->frame);
#endif
    }
}

bool taskWaitFor(Task** ret) {
    Task* task = criticalEnter();
    assert(task != NULL);
    if (saveStateToFrame(&task->frame)) {
        *ret = task;
        moveTaskToState(task, WAITING);
        enqueueTask(task);
        return true;
    } else {
        return false;
    }
}

