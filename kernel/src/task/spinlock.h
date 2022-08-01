#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int spinlock;
    struct HartFrame_s* locked_by;
    size_t num_locks;
    struct TrapFrame_s* crit_ret_frame;
} SpinLock;

void lockSpinLock(SpinLock* lock);

bool tryLockingSpinLock(SpinLock* lock);

void unlockSpinLock(SpinLock* lock);

#endif
