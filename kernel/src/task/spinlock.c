
#include <assert.h>

#include "task/spinlock.h"

#include "task/harts.h"
#include "task/syscall.h"
#include "task/task.h"

void initSpinLock(SpinLock* lock) {
    lock->spinlock.lock = 0;
    lock->locked_by = NULL;
    lock->num_locks = 0;
}

void lockSpinLock(SpinLock* lock) {
    TrapFrame* frame = criticalEnter();
    HartFrame* hart = getCurrentHartFrame();
    if (hart == NULL || lock->locked_by != hart) {
        lockUnsafeLock(&lock->spinlock);
    }
    lock->num_locks++;
    lock->locked_by = hart;
    lock->crit_ret_frame = frame;
#ifdef DEBUG
    if (hart != NULL) {
        hart->spinlocks_locked++;
    }
#endif
}

bool tryLockingSpinLock(SpinLock* lock) {
    TrapFrame* frame = criticalEnter();
    HartFrame* hart = getCurrentHartFrame();
    if ((hart != NULL && lock->locked_by == hart) || tryLockingUnsafeLock(&lock->spinlock)) {
        lock->num_locks++;
        lock->locked_by = hart;
        lock->crit_ret_frame = frame;
#ifdef DEBUG
        if (hart != NULL) {
            hart->spinlocks_locked++;
        }
#endif
        return true;
    } else {
        criticalReturn(frame);
        return false;
    }
}

void unlockSpinLock(SpinLock* lock) {
    TrapFrame* frame = lock->crit_ret_frame;
    lock->num_locks--;
    if (lock->num_locks == 0) {
        lock->locked_by = NULL;
        unlockUnsafeLock(&lock->spinlock);
#ifdef DEBUG
        HartFrame* hart = getCurrentHartFrame();
        if (hart != NULL) {
            hart->spinlocks_locked--;
        }
#endif
        criticalReturn(frame);
    }
}

