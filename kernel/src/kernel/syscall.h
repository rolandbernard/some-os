#ifndef _KERNEL_SYSCALL_H_
#define _KERNEL_SYSCALL_H_

#include "interrupt/syscall.h"

void timesSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#endif
