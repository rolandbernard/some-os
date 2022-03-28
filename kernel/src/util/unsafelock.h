#ifndef _UNSAFE_LOCK_H_
#define _UNSAFE_LOCK_H_

#include <stdbool.h>

typedef int UnsafeLock;

// Lock simple spinlock with fences
void lockUnsafeLock(UnsafeLock* lock);

// Try to lock, and return true if locking succeeded, false if it fails
bool tryLockingUnsafeLock(UnsafeLock* lock);

// Unlock simple spinlock with fences
void unlockUnsafeLock(UnsafeLock* lock);

#endif
