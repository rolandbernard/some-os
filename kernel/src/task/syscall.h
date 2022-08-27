#ifndef _TASK_SYSCALL_H_
#define _TASK_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn yieldSyscall(TrapFrame* frame);

SyscallReturn sleepSyscall(TrapFrame* frame);

SyscallReturn criticalSyscall(TrapFrame* frame);

Task* criticalEnter();

void criticalReturn(Task* to);

// Enter a critical section, put the current task into a waiting state, save it into task and return
// true. When the task will be entered again, false will be returned.
bool taskWaitFor(Task** task);

#endif
