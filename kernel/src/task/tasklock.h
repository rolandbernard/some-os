#ifndef _PROCESS_LOCK_H_
#define _PROCESS_LOCK_H_

#include <stdbool.h>

#include "task/types.h"
#include "util/unsafelock.h"

typedef struct TaskLock_s {
    UnsafeLock unsafelock;
    Task* locked_by;
    size_t num_locks;
    Task* wait_queue;
#ifdef DEBUG
    uintptr_t locked_at;
    struct TaskLock_s* next_locked;
    struct TaskLock_s* prev_locked;
#endif
} TaskLock;

void initTaskLock(TaskLock* lock);

void lockTaskLock(TaskLock* lock);

bool tryLockingTaskLock(TaskLock* lock);

void unlockTaskLock(TaskLock* lock);

#endif
