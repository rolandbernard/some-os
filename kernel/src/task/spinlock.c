
#include <assert.h>

#include "task/spinlock.h"

#include "task/harts.h"
#include "task/syscall.h"
#include "task/task.h"

static bool panic_lock_bypass = false;

void initSpinLock(SpinLock* lock) {
    lock->unsafelock.lock = 0;
    lock->locked_by = NULL;
    lock->num_locks = 0;
}

void lockSpinLock(SpinLock* lock) {
    Task* task = criticalEnter();
    HartFrame* hart = getCurrentHartFrame();
    if (hart == NULL || lock->locked_by != hart) {
        if (!panic_lock_bypass) {
            lockUnsafeLock(&lock->unsafelock);
        }
        lock->locked_by = hart;
        lock->crit_ret_frame = task;
#ifdef DEBUG
        lock->locked_at = (uintptr_t)__builtin_return_address(0);
#endif
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
    if ((hart != NULL && lock->locked_by == hart) || panic_lock_bypass || tryLockingUnsafeLock(&lock->unsafelock)) {
        lock->num_locks++;
        lock->locked_by = hart;
        lock->crit_ret_frame = task;
#ifdef DEBUG
        lock->locked_at = (uintptr_t)__builtin_return_address(0);
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
    assert(hart == lock->locked_by || panic_lock_bypass);
#endif
    lock->num_locks--;
    if (lock->num_locks == 0) {
        lock->locked_by = NULL;
#ifdef DEBUG
        lock->locked_at = 0;
#endif
        Task* crit_return = lock->crit_ret_frame;
        lock->crit_ret_frame = NULL;
        if (!panic_lock_bypass) {
            unlockUnsafeLock(&lock->unsafelock);
        }
        criticalReturn(crit_return);
    }
}

void panicUnlock() {
    panic_lock_bypass = true;
}

