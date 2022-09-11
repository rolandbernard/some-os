
#include <assert.h>

#include "task/syscall.h"

#include "task/harts.h"
#include "task/schedule.h"

SyscallReturn yieldSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL); // Only a tasks can be yielded
    // Do nothing, task will be enqueued
    SYSCALL_RETURN(0);
}

bool handleSleepWakeup(Task* task) {
    Time time = getTime();
    bool wakeup = time >= task->times.entered;
    if (!wakeup && task->process != NULL) {
        lockSpinLock(&task->process->lock); 
        wakeup = task->process->signals.signals != NULL;
        unlockSpinLock(&task->process->lock); 
    }
    if (wakeup) {
        if (time >= task->times.entered) {
            task->frame.regs[REG_ARGUMENT_0] = 0;
        } else {
            task->frame.regs[REG_ARGUMENT_0] = (task->times.entered - time) * (1000000000UL / CLOCKS_PER_SEC);
        }
        return true;
    } else {
        return false;
    }
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
        lockSpinLock(&task->sched.lock); 
        task->times.entered = end;
        task->sched.wakeup_function = handleSleepWakeup;
        moveTaskToState(task, SLEEPING);
        unlockSpinLock(&task->sched.lock); 
        enqueueTask(task);
        setTimeoutTime(end, NULL, NULL); // Make sure we wake up in time
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
        assert(getCurrentHartFrame()->spinlocks_locked == 0);
        criticalReturnFastPath(to);
#else
        swapTrapFrame(getCurrentTrapFrame(), &to->frame);
#endif
    }
}

static void taskLeave(void* _, Task* task) {
    moveTaskToState(task, TERMINATED);
    enqueueTask(task);
    runNextTask();
}

noreturn void leave() {
    Task* task = criticalEnter();
    assert(task != NULL);
    callInHart((void*)taskLeave, task);
}

