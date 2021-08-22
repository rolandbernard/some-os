#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

typedef int SpinLock;

// Lock simple spinlock with fences
void lockSpinLock(SpinLock* lock);

// Unlock simple spinlock with fences
void unlockSpinLock(SpinLock* lock);

#endif
