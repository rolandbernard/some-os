
#include <assert.h>

#include "task/tasklock.h"

#include "task/schedule.h"
#include "task/syscall.h"
#include "task/task.h"

#ifdef DEBUG
static SpinLock locked_lock;
static TaskLock* locked_locks;
#endif

void initTaskLock(TaskLock* lock) {
    lock->unsafelock.lock = 0;
    lock->locked_by = NULL;
    lock->num_locks = 0;
    lock->wait_queue = NULL;
}

static void waitForTaskLock(void* _, Task* task, TaskLock* lock) {
    moveTaskToState(task, WAITING);
    enqueueTask(task);
    task->sched.sched_next = lock->wait_queue;
    lock->wait_queue = task;
    unlockUnsafeLock(&lock->unsafelock);
    runNextTask();
}

static void basicLockByTask(TaskLock* lock, Task* task) {
    lock->locked_by = task;
#ifdef DEBUG
    lockSpinLock(&locked_lock);
    lock->prev_locked = NULL;
    lock->next_locked = locked_locks;
    if (locked_locks != NULL) {
        locked_locks->prev_locked = lock;
    }
    locked_locks = lock;
    unlockSpinLock(&locked_lock);
#endif
}

static void basicUnlockByTask(TaskLock* lock, Task* task) {
    lock->locked_by = NULL;
#ifdef DEBUG
    lockSpinLock(&locked_lock);
    if (lock->prev_locked != NULL) {
        lock->prev_locked->next_locked = lock->next_locked;
    } else {
        locked_locks = lock->next_locked;
    }
    if (lock->next_locked != NULL) {
        lock->next_locked->prev_locked = lock->prev_locked;
    }
    unlockSpinLock(&locked_lock);
#endif
}

static bool lockOrWaitTaskLock(TaskLock* lock) {
    bool result;
    Task* task = criticalEnter();
    assert(task != NULL);
    lockUnsafeLock(&lock->unsafelock);
    if (lock->locked_by == NULL) {
        basicLockByTask(lock, task);
        unlockUnsafeLock(&lock->unsafelock);
        criticalReturn(task);
        result = true;
    } else {
        if (saveToFrame(&task->frame)) {
            callInHart((void*)waitForTaskLock, task, lock);
        }
        result = false;
    }
    return result;
}

void lockTaskLock(TaskLock* lock) {
    Task* task = getCurrentTask();
    assert(task != NULL);
    if (lock->locked_by != task) {
        // Wait until we are able to lock.
        while (!lockOrWaitTaskLock(lock));
#ifdef DEBUG
        lock->locked_at = (uintptr_t)__builtin_return_address(0);
#endif
    }
    lock->num_locks++;
}

bool tryLockingTaskLock(TaskLock* lock) {
    Task* task = getCurrentTask();
    assert(task != NULL);
    bool result = true;
    if (lock->locked_by != task) {
        task = criticalEnter();
        lockUnsafeLock(&lock->unsafelock);
        if (lock->locked_by == NULL) {
            basicLockByTask(lock, task);
            result = true;
#ifdef DEBUG
            lock->locked_at = (uintptr_t)__builtin_return_address(0);
#endif
        } else {
            result = false;
        }
        unlockUnsafeLock(&lock->unsafelock);
        criticalReturn(task);
    }
    if (result) {
        lock->num_locks++;
    }
    return result;
}

void unlockTaskLock(TaskLock* lock) {
    assert(getCurrentTask() != NULL);
    assert(getCurrentTask() == lock->locked_by);
    lock->num_locks--;
    if (lock->num_locks == 0) {
#ifdef DEBUG
        lock->locked_at = 0;
#endif
        Task* task = criticalEnter();
        lockUnsafeLock(&lock->unsafelock);
        basicUnlockByTask(lock, task);
        while (lock->wait_queue != NULL) {
            Task* wakeup = lock->wait_queue;
            lock->wait_queue = wakeup->sched.sched_next;
            moveTaskToState(wakeup, ENQUABLE);
            enqueueTask(wakeup);
        }
        unlockUnsafeLock(&lock->unsafelock);
        criticalReturn(task);
    }
}

