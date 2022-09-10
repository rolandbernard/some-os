
#include <assert.h>

#include "task/schedule.h"

#include "error/log.h"
#include "interrupt/trap.h"
#include "process/process.h"
#include "process/signals.h"
#include "task/harts.h"
#include "task/spinlock.h"
#include "task/task.h"
#include "task/types.h"
#include "util/util.h"

#define PRIORITY_DECREASE (CLOCKS_PER_SEC / 75)

SpinLock sleeping_lock;
Task* sleeping = NULL;

static void addSleepingTask(Task* task) {
    lockSpinLock(&sleeping_lock);
    assert(task != sleeping);
    task->sched.sched_next = sleeping;
    sleeping = task;
    unlockSpinLock(&sleeping_lock);
}

void enqueueTask(Task* task) {
    HartFrame* hart = task->frame.hart;
    if (hart == NULL) {
        // Prefer the last hart of the process, but if NULL enqueue for the current
        hart = getCurrentHartFrame();
    }
    assert(hart != NULL);
    ScheduleQueue* queue = &hart->queue;
    if (hart->idle_task != task) { // Ignore the idle process
        lockSpinLock(&task->sched.lock);
        switch (task->sched.state) {
            case SLEEPING:
            case PAUSED:
            case WAIT_CHLD:
                addSleepingTask(task);
                unlockSpinLock(&task->sched.lock);
                break;
            case ENQUABLE:
                moveTaskToState(task, READY);
                if (task->sched.run_for > PRIORITY_DECREASE) {
                    // Lower priority to not starve other processes
                    task->sched.queue_priority =
                        task->sched.priority + umin(task->sched.run_for / PRIORITY_DECREASE, 5);
                    task->sched.run_for -= PRIORITY_DECREASE / 4;
                } else {
                    task->sched.queue_priority = task->sched.priority;
                }
                pushTaskToQueue(queue, task);
                unlockSpinLock(&task->sched.lock);
                break;
            case READY: // Task has already been enqueued
            case RUNNING: // It is currently running
            case WAITING: // Task is already handled somewhere else
                unlockSpinLock(&task->sched.lock);
                break;
            case TERMINATED: // Task can not be enqueue, it is only waiting to be freed.
                unlockSpinLock(&task->sched.lock);
                deallocTask(task);
                break;
            default:        // We don't know what to do
                panic();    // These should not happen
        }
    }
}

static void awakenTask(Task* task) {
    lockSpinLock(&task->sched.lock);
    if (task->sched.state == SLEEPING) {
        Time time = getTime();
        if (time >= task->sched.sleeping_until) {
            task->frame.regs[REG_ARGUMENT_0] = 0;
        } else {
            task->frame.regs[REG_ARGUMENT_0] = task->sched.sleeping_until - time;
        }
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINTR;
    }
    if (task->process != NULL) {
        handleProcessTaskWakeup(task);
    }
    unlockSpinLock(&task->sched.lock);
    moveTaskToState(task, ENQUABLE);
    enqueueTask(task);
}

static void awakenTasks() {
    lockSpinLock(&sleeping_lock);
    Time time = getTime();
    Task** current = &sleeping;
    while (*current != NULL) {
        Task* task = *current;
        lockSpinLock(&task->sched.lock);
        if (
            (task->sched.state == SLEEPING && task->sched.sleeping_until <= time)
            || (task->process != NULL && shouldTaskWakeup(task))
            || task->sched.state == TERMINATED
        ) {
            unlockSpinLock(&task->sched.lock);
            *current = task->sched.sched_next;
            awakenTask(task);
        } else {
            unlockSpinLock(&task->sched.lock);
            current = &task->sched.sched_next;
        }
    }
    unlockSpinLock(&sleeping_lock);
}

noreturn void runNextTask() {
    Task* current = NULL;
    current = getCurrentTask();
    if (current != NULL) {
        // If this is called from inside a process. Call exit syscall.
        syscall(SYSCALL_EXIT);
        panic();
    } else {
        runNextTaskFrom(getCurrentHartFrame());
    }
}

noreturn void runNextTaskFrom(HartFrame* hart) {
    for (;;) {
        awakenTasks();
        Task* next = NULL;
        while (next == NULL) {
            next = pullTaskForHart(hart);
            lockSpinLock(&next->sched.lock);
            if (next->sched.state == TERMINATED) {
                unlockSpinLock(&next->sched.lock);
                deallocTask(next);
                next = NULL;
            } else {
                unlockSpinLock(&next->sched.lock);
            }
        }
        assert(next != NULL);
        if (next->process == NULL || handlePendingSignals(next)) {
            enterTask(next);
        } else {
            enqueueTask(next);
        }
    }
}

Task* pullTaskForHart(HartFrame* hart) {
    assert(hart != NULL);
    HartFrame* current = hart;
    do {
        Task* task = pullTaskFromQueue(&current->queue);
        if (task != NULL) {
            return task;
        } else {
            current = current->next;
        }
    } while (current != hart);
    return hart->idle_task;
}

Task* pullTaskFromQueue(ScheduleQueue* queue) {
    lockSpinLock(&queue->lock);
    if (queue->head == NULL) {
        unlockSpinLock(&queue->lock);
        return NULL;
    } else {
        Task* ret = queue->head;
        queue->head = ret->sched.sched_next;
        if (queue->tails[ret->sched.queue_priority] == ret) {
            if (ret->sched.queue_priority == 0) {
                queue->tails[0] = NULL;
            } else {
                queue->tails[ret->sched.queue_priority] = queue->tails[ret->sched.queue_priority - 1];
            }
            for (Priority i = ret->sched.queue_priority + 1; i < MAX_PRIORITY && queue->tails[i] == ret; i++) {
                queue->tails[i] = queue->tails[i - 1];
            }
        }
        unlockSpinLock(&queue->lock);
        return ret;
    }
}

void pushTaskToQueue(ScheduleQueue* queue, Task* process) {
    if (process->sched.queue_priority > LOWEST_PRIORITY) {
        process->sched.queue_priority = LOWEST_PRIORITY;
    }
    lockSpinLock(&queue->lock);
    if (queue->tails[process->sched.queue_priority] == NULL) {
        process->sched.sched_next = queue->head;
        queue->head = process;
    } else {
        process->sched.sched_next = queue->tails[process->sched.queue_priority]->sched.sched_next;
        queue->tails[process->sched.queue_priority]->sched.sched_next = process;
    }
    Task* old = queue->tails[process->sched.queue_priority];
    for (Priority i = process->sched.queue_priority; i < MAX_PRIORITY && queue->tails[i] == old; i++) {
        queue->tails[i] = process;
    }
    unlockSpinLock(&queue->lock);
}

Task* removeTaskFromQueue(ScheduleQueue* queue, Task* task) {
    lockSpinLock(&queue->lock);
    Task** current = &queue->head;
    while (*current != NULL) {
        if (*current == task) {
            *current = (*current)->sched.sched_next;
            unlockSpinLock(&queue->lock);
            return task;
        }
        *current = NULL;
    }
    unlockSpinLock(&queue->lock);
    return NULL;
}

void moveTaskToState(Task* task, TaskState state) {
    assert(task != NULL);
    lockSpinLock(&task->sched.lock);
    if (task->sched.state != TERMINATED) {
        task->sched.state = state;
    }
    unlockSpinLock(&task->sched.lock);
}

