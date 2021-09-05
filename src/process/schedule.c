
#include <assert.h>

#include "process/schedule.h"

#include "interrupt/trap.h"
#include "error/log.h"
#include "process/process.h"

void enqueueProcess(Process* process) {
    assert(process->frame.hart != NULL);
    ScheduleQueue* queue = &process->frame.hart->queue;
    if (process->state == WAITING) {
        // Don't do anything. Should be tracked somewhere else.
    } else if (process->state == TERMINATED) {
        freeProcess(process);
    } else {
        process->state = READY;
        pushProcessToQueue(queue, process);
    }
    Process* next = pullProcessFromQueue(queue);
    if (next != NULL) {
        enterProcess(next);
    } else {
        // TODO: Create and enter idle process
    }
}

Process* pullProcessFromQueue(ScheduleQueue* queue) {
    if (queue->head == NULL) {
        return NULL;
    } else {
        Process* ret = queue->head;
        queue->head = NULL;
        if (queue->tails[ret->priority] == ret) {
            if (ret->priority == 0) {
                queue->tails[0] = NULL;
            } else {
                queue->tails[ret->priority] = queue->tails[ret->priority - 1];
            }
        }
        return ret;
    }
}

void pushProcessToQueue(ScheduleQueue* queue, Process* process) {
    if (process->priority >= MAX_PRIORITY) {
        process->priority = MAX_PRIORITY - 1;
    }
    if (queue->tails[process->priority] == NULL) {
        process->next = queue->head;
        queue->head = process;
    } else {
        process->next = queue->tails[process->priority];
        queue->tails[process->priority] = process;
    }
    queue->tails[process->priority] = process;
}

Process* removeProccesFromQueue(ScheduleQueue* queue, Process* process) {
    Process** current = &queue->head;
    while (*current != NULL) {
        if (*current == process) {
            *current = (*current)->next;
            return process;
        }
        *current = NULL;
    }
    return NULL;
}

