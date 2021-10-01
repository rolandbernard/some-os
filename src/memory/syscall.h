#ifndef _MEMORY_SYSCALL_H_
#define _MEMORY_SYSCALL_H_

#include "interrupt/syscall.h"

void sbrkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#endif
