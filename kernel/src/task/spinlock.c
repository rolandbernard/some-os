
#include <assert.h>

#include "task/spinlock.h"

#include "task/harts.h"
#include "task/syscall.h"
#include "task/task.h"

void initSpinLock(SpinLock* lock) {
    lock->unsafelock.lock = 0;
    lock->locked_by = NULL;
    lock->num_locks = 0;
}

void lockSpinLock(SpinLock* lock) {
    Task* task = criticalEnter();
    HartFrame* hart = getCurrentHartFrame();
    if (hart == NULL || lock->locked_by != hart) {
        lockUnsafeLock(&lock->unsafelock);
        lock->locked_by = hart;
        lock->crit_ret_frame = task;
    }
    lock->num_locks++;
#ifdef DEBUG
    if (hart != NULL) {
        hart->spinlocks_locked++;
    }
#endif
}

bool tryLockingSpinLock(SpinLock* lock) {
    Task* task = criticalEnter();
    HartFrame* hart = getCurrentHartFrame();
    if ((hart != NULL && lock->locked_by == hart) || tryLockingUnsafeLock(&lock->unsafelock)) {
        lock->num_locks++;
        lock->locked_by = hart;
        lock->crit_ret_frame = task;
#ifdef DEBUG
        if (hart != NULL) {
            hart->spinlocks_locked++;
        }
#endif
        return true;
    } else {
        criticalReturn(task);
        return false;
    }
}

void unlockSpinLock(SpinLock* lock) {
#ifdef DEBUG
    HartFrame* hart = getCurrentHartFrame();
    if (hart != NULL) {
        hart->spinlocks_locked--;
    }
    assert(hart == lock->locked_by);
#endif
    lock->num_locks--;
    if (lock->num_locks == 0) {
        lock->locked_by = NULL;
        unlockUnsafeLock(&lock->unsafelock);
        criticalReturn(lock->crit_ret_frame);
    }
}

