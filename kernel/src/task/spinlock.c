
#include <assert.h>

#include "task/spinlock.h"

#include "util/unsafelock.h"
#include "task/task.h"
#include "task/harts.h"

void lockSpinLock(SpinLock* lock) {
    assert(getCurrentTask() == NULL);
    lockUnsafeLock(&lock->spinlock);
#ifdef DEBUG
    HartFrame* hart = getCurrentHartFrame();
    if (hart != NULL) {
        hart->spinlocks_locked++;
    }
#endif
}

bool tryLockingSpinLock(SpinLock* lock) {
    assert(getCurrentTask() == NULL);
    bool res = tryLockingUnsafeLock(&lock->spinlock);
#ifdef DEBUG
    HartFrame* hart = getCurrentHartFrame();
    if (res && hart != NULL) {
        hart->spinlocks_locked++;
    }
#endif
    return res;
}

void unlockSpinLock(SpinLock* lock) {
    assert(getCurrentTask() == NULL);
    unlockUnsafeLock(&lock->spinlock);
#ifdef DEBUG
    HartFrame* hart = getCurrentHartFrame();
    if (hart != NULL) {
        hart->spinlocks_locked--;
    }
#endif
}

