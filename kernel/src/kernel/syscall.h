#ifndef _KERNEL_SYSCALL_H_
#define _KERNEL_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn timesSyscall(TrapFrame* frame);

#endif
