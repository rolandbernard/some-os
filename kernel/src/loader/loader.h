#ifndef _LOADER_H_
#define _LOADER_H_

#include "process/types.h"
#include "interrupt/syscall.h"
#include "memory/virtptr.h"
#include "files/vfs/types.h"

#define USER_STACK_TOP (1UL << 38)
#define USER_STACK_SIZE (1UL << 19)

typedef void (*ProgramLoadCallback)(Error error, void* udata);

Error loadProgramInto(Task* task, const char* path, VirtPtr args, VirtPtr envs);

SyscallReturn execveSyscall(TrapFrame* frame);

#endif
