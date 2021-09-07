#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#include <stdbool.h>

typedef int SpinLock;

// Lock simple spinlock with fences
void lockSpinLock(SpinLock* lock);

// Try to lock, and return true if locking succeeded, false if it fails
bool tryLockingSpinLock(SpinLock* lock);

// Unlock simple spinlock with fences
void unlockSpinLock(SpinLock* lock);

#endif
