#ifndef _TASK_SYSCALL_H_
#define _TASK_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn yieldSyscall(TrapFrame* frame);

SyscallReturn sleepSyscall(TrapFrame* frame);

SyscallReturn criticalSyscall(TrapFrame* frame);

TrapFrame* criticalEnter();

void criticalReturn(TrapFrame* to);

#endif
