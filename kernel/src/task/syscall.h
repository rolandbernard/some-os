#ifndef _TASK_SYSCALL_H_
#define _TASK_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn yieldSyscall(TrapFrame* frame);

SyscallReturn sleepSyscall(TrapFrame* frame);

SyscallReturn criticalSyscall(TrapFrame* frame);

Task* criticalEnter();

void criticalReturn(Task* to);

#endif
