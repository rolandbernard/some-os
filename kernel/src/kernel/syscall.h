#ifndef _KERNEL_SYSCALL_H_
#define _KERNEL_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn timesSyscall(TrapFrame* frame);

SyscallReturn getNanosecondsSyscall(TrapFrame* frame);

SyscallReturn setNanosecondsSyscall(TrapFrame* frame);

#endif
