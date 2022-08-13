#ifndef _MEMORY_SYSCALL_H_
#define _MEMORY_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn sbrkSyscall(TrapFrame* frame);

SyscallReturn protectSyscall(TrapFrame* frame);

#endif
