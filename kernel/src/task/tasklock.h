#ifndef _PROCESS_LOCK_H_
#define _PROCESS_LOCK_H_

#include <stdbool.h>

#include "task/types.h"
#include "util/unsafelock.h"

typedef struct {
    UnsafeLock unsafelock;
    Task* locked_by;
    size_t num_locks;
    Task* wait_queue;
} TaskLock;

void initTaskLock(TaskLock* lock);

void lockTaskLock(TaskLock* lock);

bool tryLockingTaskLock(TaskLock* lock);

void unlockTaskLock(TaskLock* lock);

#endif
