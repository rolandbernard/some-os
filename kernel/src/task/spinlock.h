#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <stdbool.h>
#include <stddef.h>

#include "util/unsafelock.h"

typedef struct {
    UnsafeLock unsafelock;
    struct HartFrame_s* locked_by;
    size_t num_locks;
    struct TrapFrame_s* crit_ret_frame;
} SpinLock;

void initSpinLock(SpinLock* lock);

void lockSpinLock(SpinLock* lock);

bool tryLockingSpinLock(SpinLock* lock);

void unlockSpinLock(SpinLock* lock);

#endif
