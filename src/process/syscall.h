#ifndef _PROCESS_SYSCALL_H_
#define _PROCESS_SYSCALL_H_

#include "interrupt/syscall.h"

uintptr_t exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

uintptr_t yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void exit();

void yield();

#endif
