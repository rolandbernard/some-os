
#include <assert.h>

#include "task/tasklock.h"

#include "util/unsafelock.h"
#include "task/task.h"

void initTaskLock(TaskLock* lock) {
    lock->spinlock = 0;
    lock->locked_by = NULL;
    lock->num_locks = 0;
}

void lockTaskLock(TaskLock* lock) {
    Task* task = getCurrentTask();
    assert(task != NULL);
    if (lock->locked_by != task) {
        lockUnsafeLock(&lock->spinlock);
    }
    lock->num_locks++;
    lock->locked_by = task;
}

bool tryLockingTaskLock(TaskLock* lock) {
    Task* task = getCurrentTask();
    assert(task != NULL);
    if (lock->locked_by == task || tryLockingUnsafeLock(&lock->spinlock)) {
        lock->num_locks++;
        lock->locked_by = task;
        return true;
    } else {
        return false;
    }
}

void unlockTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    lock->num_locks--;
    if (lock->num_locks == 0) {
        lock->locked_by = NULL;
        unlockUnsafeLock(&lock->spinlock);
    }
}

