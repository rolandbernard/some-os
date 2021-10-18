#ifndef _FILES_SYSCALL_H_
#define _FILES_SYSCALL_H_

#include "interrupt/syscall.h"

void openSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void linkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void unlinkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void renameSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void closeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void readSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void writeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void seekSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void statSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void dupSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void truncSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void chmodSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void chownSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void mountSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void umountSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void readdirSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void chdirSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

void getcwdSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

char* copyPathFromSyscallArgs(Process* process, uintptr_t ptr);

#endif
