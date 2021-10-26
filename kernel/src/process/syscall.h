#ifndef _PROCESS_SYSCALL_H_
#define _PROCESS_SYSCALL_H_

#include "interrupt/syscall.h"

void forkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void sleepSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void getpidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void getppidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void waitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void sigactionSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void sigreturnSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void killSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void setUidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void setGidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void getUidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void getGidSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void exit();

#endif
