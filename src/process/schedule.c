
#include <assert.h>

#include "interrupt/syscall.h"
#include "process/harts.h"
#include "process/schedule.h"

#include "interrupt/trap.h"
#include "error/log.h"
#include "process/process.h"
#include "util/util.h"

#define PRIORITY_DECREASE 64

void enqueueProcess(Process* process) {
    HartFrame* hart = process->frame.hart;
    if (hart == NULL) {
        // Prefer the last hart of the process, but if NULL enqueue for the current
        hart = getCurrentHartFrame();
    }
    assert(hart != NULL);
    ScheduleQueue* queue = &hart->queue;
    if (hart->idle_process != process) { // Ignore the idle process
        if (process->sched.state == TERMINATED) {
            freeProcess(process);
        } else if (process->sched.state == ENQUEUEABLE) {
            moveToSchedState(process, READY);
            if (process->sched.runs % PRIORITY_DECREASE == 0) {
                process->sched.queue_priority =
                    process->sched.priority + (process->sched.runs / PRIORITY_DECREASE);
            } else {
                process->sched.queue_priority = process->sched.priority;
            }
            pushProcessToQueue(queue, process);
        }
    }
}

void runNextProcess() {
    Process* current = getCurrentProcess();
    if (current != NULL) {
        // If this is called from inside a process. Call exit syscall.
        syscall(SYSCALL_EXIT);
    } else {
        runNextProcessFrom(getCurrentHartFrame());
    }
}

void runNextProcessFrom(HartFrame* hart) {
    Process* next = pullProcessForHart(hart);
    next->sched.runs++;
    assert(next != NULL);
    enterProcess(next);
}

Process* pullProcessForHart(HartFrame* hart) {
    assert(hart != NULL);
    HartFrame* current = hart;
    do {
        Process* process = pullProcessFromQueue(&current->queue);
        if (process != NULL) {
            return process;
        } else {
            current = hart->next;
        }
    } while (current != hart);
    return hart->idle_process;
}

Process* pullProcessFromQueue(ScheduleQueue* queue) {
    lockSpinLock(&queue->lock);
    if (queue->head == NULL) {
        unlockSpinLock(&queue->lock);
        return NULL;
    } else {
        Process* ret = queue->head;
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
        moveToSchedState(ret, ENQUEUEABLE);
        return ret;
    }
}

void pushProcessToQueue(ScheduleQueue* queue, Process* process) {
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
    Process* old = queue->tails[process->sched.queue_priority];
    for (Priority i = process->sched.queue_priority; i < MAX_PRIORITY && queue->tails[i] == old; i++) {
        queue->tails[i] = process;
    }
    unlockSpinLock(&queue->lock);
}

Process* removeProccesFromQueue(ScheduleQueue* queue, Process* process) {
    lockSpinLock(&queue->lock);
    Process** current = &queue->head;
    while (*current != NULL) {
        if (*current == process) {
            *current = (*current)->sched.sched_next;
            unlockSpinLock(&queue->lock);
            return process;
        }
        *current = NULL;
    }
    unlockSpinLock(&queue->lock);
    return NULL;
}

bool moveToSchedState(Process* process, ProcessState state) {
    if (process->sched.state == FREED || process->sched.state == TERMINATED) {
        // Can't set the new state
        return false;
    } else {
        process->sched.state = state;
        return true;
    }
}

