#ifndef _PROCESS_SYSCALL_H_
#define _PROCESS_SYSCALL_H_

#include "interrupt/syscall.h"

void forkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void sleepSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void exit();

#endif
