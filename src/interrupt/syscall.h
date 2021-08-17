#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include "process/process.h"

void* syscall(int kind, ...);

typedef void* SyscallArgs[7];
typedef void* (*SyscallFunction)(Process* process, SyscallArgs args);

void registerSyscall(int kind, SyscallFunction function);

void runSyscall(Process* process);

#endif
