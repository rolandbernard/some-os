#ifndef _MEMORY_SYSCALL_H_
#define _MEMORY_SYSCALL_H_

#include "interrupt/syscall.h"

void sbrkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#define PROT_NONE 0
#define PROT_READ 4
#define PROT_WRITE 2
#define PROT_EXEC 1
#define PROT_READ_WRITE_EXEC (PROT_READ | PROT_WRITE | PROT_EXEC)

void protectSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#endif
