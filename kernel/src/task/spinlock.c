
#include <assert.h>

#include "task/spinlock.h"

#include "task/harts.h"
#include "task/syscall.h"
#include "task/task.h"
#include "util/unsafelock.h"

void lockSpinLock(SpinLock* lock) {
    TrapFrame* frame = criticalEnter();
    lockUnsafeLock(&lock->spinlock);
    lock->locked_by = frame;
#ifdef DEBUG
    HartFrame* hart = getCurrentHartFrame();
    if (hart != NULL) {
        hart->spinlocks_locked++;
    }
#endif
}

bool tryLockingSpinLock(SpinLock* lock) {
    TrapFrame* frame = criticalEnter();
    bool res = tryLockingUnsafeLock(&lock->spinlock);
    if (res) {
        lock->locked_by = frame;
    } else {
        criticalReturn(frame);
    }
#ifdef DEBUG
    HartFrame* hart = getCurrentHartFrame();
    if (res && hart != NULL) {
        hart->spinlocks_locked++;
    }
#endif
    return res;
}

void unlockSpinLock(SpinLock* lock) {
    TrapFrame* frame = lock->locked_by;
    unlockUnsafeLock(&lock->spinlock);
#ifdef DEBUG
    HartFrame* hart = getCurrentHartFrame();
    if (hart != NULL) {
        hart->spinlocks_locked--;
    }
#endif
    criticalReturn(frame);
}

