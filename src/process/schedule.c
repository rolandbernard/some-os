
#include <assert.h>

#include "interrupt/syscall.h"
#include "process/harts.h"
#include "process/schedule.h"

#include "interrupt/trap.h"
#include "error/log.h"
#include "process/process.h"
#include "util/util.h"

void enqueueProcess(Process* process) {
    assert(process->frame.hart != NULL);
    HartFrame* hart = process->frame.hart;
    ScheduleQueue* queue = &hart->queue;
    if (hart->idle_process != process) { // Ignore the idle process
        if (process->state == WAITING) {
            // Don't do anything. Waiting processes should be tracked somewhere else.
        } else if (process->state == TERMINATED) {
            freeProcess(process);
        } else if (process->state == KILLED) {
            // This is propably an error
            KERNEL_LOG("[!] Killed process %li was enqueued", process->pid);
        } else {
            process->state = READY;
            if (process->sched_priority < process->priority) {
                process->sched_priority = process->priority;
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
    next->sched_priority = next->priority;
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
        queue->head = ret->sched_next;
        if (queue->tails[ret->sched_priority] == ret) {
            if (ret->sched_priority == 0) {
                queue->tails[0] = NULL;
            } else {
                queue->tails[ret->sched_priority] = queue->tails[ret->sched_priority - 1];
            }
            for (Priority i = ret->sched_priority + 1; i < MAX_PRIORITY && queue->tails[i] == ret; i++) {
                queue->tails[i] = queue->tails[i - 1];
            }
        }
        unlockSpinLock(&queue->lock);
        return ret;
    }
}

void pushProcessToQueue(ScheduleQueue* queue, Process* process) {
    if (process->sched_priority > LOWEST_PRIORITY) {
        process->sched_priority = LOWEST_PRIORITY;
    }
    lockSpinLock(&queue->lock);
    if (queue->tails[process->sched_priority] == NULL) {
        process->sched_next = queue->head;
        queue->head = process;
    } else {
        process->sched_next = queue->tails[process->sched_priority]->sched_next;
        queue->tails[process->sched_priority]->sched_next = process;
    }
    Process* old = queue->tails[process->sched_priority];
    for (Priority i = process->sched_priority; i < MAX_PRIORITY && queue->tails[i] == old; i++) {
        queue->tails[i] = process;
    }
    unlockSpinLock(&queue->lock);
}

Process* removeProccesFromQueue(ScheduleQueue* queue, Process* process) {
    lockSpinLock(&queue->lock);
    Process** current = &queue->head;
    while (*current != NULL) {
        if (*current == process) {
            *current = (*current)->sched_next;
            unlockSpinLock(&queue->lock);
            return process;
        }
        *current = NULL;
    }
    unlockSpinLock(&queue->lock);
    return NULL;
}

