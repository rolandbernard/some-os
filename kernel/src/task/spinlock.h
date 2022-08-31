#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/unsafelock.h"

typedef struct {
    UnsafeLock unsafelock;
    struct HartFrame_s* locked_by;
    size_t num_locks;
    struct Task_s* crit_ret_frame;
#ifdef DEBUG
    uintptr_t locked_at;
#endif
} SpinLock;

void initSpinLock(SpinLock* lock);

void lockSpinLock(SpinLock* lock);

bool tryLockingSpinLock(SpinLock* lock);

void unlockSpinLock(SpinLock* lock);

// Only for use after a panic to disable locking.
void panicUnlock();

#endif
