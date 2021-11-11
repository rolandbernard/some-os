#ifndef _TASK_SYSCALL_H_
#define _TASK_SYSCALL_H_

#include "interrupt/syscall.h"

void yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void sleepSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#endif
