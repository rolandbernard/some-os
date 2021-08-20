#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

typedef int SpinLock;

void lockSpinLock(SpinLock* lock);

void unlockSpinLock(SpinLock* lock);

#endif
