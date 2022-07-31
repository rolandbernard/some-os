#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <stdbool.h>

typedef struct {
    int spinlock;
} SpinLock;

void lockSpinLock(SpinLock* lock);

bool tryLockingSpinLock(SpinLock* lock);

void unlockSpinLock(SpinLock* lock);

#endif
