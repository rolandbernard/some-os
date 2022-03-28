#ifndef _PROCESS_LOCK_H_
#define _PROCESS_LOCK_H_

#include <stdbool.h>

typedef int TaskLock;

void lockTaskLock(TaskLock* lock);

bool tryLockingTaskLock(TaskLock* lock);

void unlockTaskLock(TaskLock* lock);

#endif
