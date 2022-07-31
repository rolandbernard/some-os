
#include <assert.h>

#include "task/tasklock.h"

#include "util/unsafelock.h"
#include "task/task.h"

void lockTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    lockUnsafeLock(&lock->spinlock);
}

bool tryLockingTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    return tryLockingUnsafeLock(&lock->spinlock);
}

void unlockTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    unlockUnsafeLock(&lock->spinlock);
}

