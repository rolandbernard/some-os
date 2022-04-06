
#include <assert.h>

#include "task/tasklock.h"

#include "util/unsafelock.h"
#include "task/task.h"

void lockTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    lockUnsafeLock(lock);
}

bool tryLockingTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    return tryLockingUnsafeLock(lock);
}

void unlockTaskLock(TaskLock* lock) {
    unlockUnsafeLock(lock);
}

