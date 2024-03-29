#ifndef _PROCESS_SYSCALL_H_
#define _PROCESS_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn forkSyscall(TrapFrame* frame);

SyscallReturn exitSyscall(TrapFrame* frame);

SyscallReturn pauseSyscall(TrapFrame* frame);

SyscallReturn alarmSyscall(TrapFrame* frame);

SyscallReturn getpidSyscall(TrapFrame* frame);

SyscallReturn getppidSyscall(TrapFrame* frame);

SyscallReturn waitSyscall(TrapFrame* frame);

SyscallReturn sigactionSyscall(TrapFrame* frame);

SyscallReturn sigreturnSyscall(TrapFrame* frame);

SyscallReturn sigpendingSyscall(TrapFrame* frame);

SyscallReturn sigprocmaskSyscall(TrapFrame* frame);

SyscallReturn killSyscall(TrapFrame* frame);

SyscallReturn setUidSyscall(TrapFrame* frame);

SyscallReturn setGidSyscall(TrapFrame* frame);

SyscallReturn getUidSyscall(TrapFrame* frame);

SyscallReturn getGidSyscall(TrapFrame* frame);

SyscallReturn getEUidSyscall(TrapFrame* frame);

SyscallReturn getEGidSyscall(TrapFrame* frame);

SyscallReturn setEUidSyscall(TrapFrame* frame);

SyscallReturn setEGidSyscall(TrapFrame* frame);

SyscallReturn setREUidSyscall(TrapFrame* frame);

SyscallReturn setREGidSyscall(TrapFrame* frame);

SyscallReturn setSidSyscall(TrapFrame* frame);

SyscallReturn getSidSyscall(TrapFrame* frame);

SyscallReturn setPgidSyscall(TrapFrame* frame);

SyscallReturn getPgidSyscall(TrapFrame* frame);

#endif
