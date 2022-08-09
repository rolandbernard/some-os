
#include <assert.h>

#include "task/tasklock.h"

#include "task/schedule.h"
#include "task/syscall.h"
#include "task/task.h"

void initTaskLock(TaskLock* lock) {
    lock->unsafelock.lock = 0;
    lock->locked_by = NULL;
    lock->num_locks = 0;
    lock->wait_queue = NULL;
}

static bool lockOrWaitTaskLock(TaskLock* lock, Task* self) {
    bool result;
    TrapFrame* lock_frame = criticalEnter();
    lockUnsafeLock(&lock->unsafelock);
    if (lock->locked_by == NULL) {
        result = true;
    } else {
        result = false;
        moveTaskToState(self, WAITING);
        self->proc_next = lock->wait_queue;
        lock->wait_queue = self;
    }
    unlockUnsafeLock(&lock->unsafelock);
    criticalReturn(lock_frame);
    return result;
}

void lockTaskLock(TaskLock* lock) {
    Task* self = getCurrentTask();
    assert(self != NULL);
    if (lock->locked_by == self) {
        lock->num_locks++;
    } else {
        while (!lockOrWaitTaskLock(lock, self)) {
            // Wait until we are able to lock.
        }
        lock->num_locks++;
        lock->locked_by = self;
    }
}

bool tryLockingTaskLock(TaskLock* lock) {
    Task* self = getCurrentTask();
    assert(self != NULL);
    if (lock->locked_by == self) {
        lock->num_locks++;
        return true;
    } else {
        bool result;
        TrapFrame* lock_frame = criticalEnter();
        lockUnsafeLock(&lock->unsafelock);
        if (lock->locked_by == NULL) {
            lock->num_locks++;
            lock->locked_by = self;
            result = true;
        } else {
            result = false;
        }
        unlockUnsafeLock(&lock->unsafelock);
        criticalReturn(lock_frame);
        return result;
    }
}

void unlockTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    lock->num_locks--;
    if (lock->num_locks == 0) {
        TrapFrame* lock_frame = criticalEnter();
        lockUnsafeLock(&lock->unsafelock);
        lock->locked_by = NULL;
        while (lock->wait_queue != NULL) {
            Task* wakeup = lock->wait_queue;
            lock->wait_queue = wakeup->proc_next;
            moveTaskToState(wakeup, ENQUABLE);
            enqueueTask(wakeup);
        }
        unlockUnsafeLock(&lock->unsafelock);
        criticalReturn(lock_frame);
    }
}

