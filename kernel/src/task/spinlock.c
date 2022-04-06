
#include <assert.h>

#include "task/spinlock.h"

#include "util/unsafelock.h"
#include "task/task.h"

void lockSpinLock(SpinLock* lock) {
    assert(getCurrentTask() == NULL);
    lockUnsafeLock(lock);
}

bool tryLockingSpinLock(SpinLock* lock) {
    assert(getCurrentTask() == NULL);
    return tryLockingUnsafeLock(lock);
}

void unlockSpinLock(SpinLock* lock) {
    unlockUnsafeLock(lock);
}

