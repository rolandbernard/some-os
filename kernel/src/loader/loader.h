#ifndef _LOADER_H_
#define _LOADER_H_

#include "process/types.h"
#include "interrupt/syscall.h"
#include "memory/virtptr.h"
#include "files/vfs.h"

typedef void (*ProgramLoadCallback)(Error error, void* udata);

void loadProgramInto(Task* task, const char* path, VirtPtr args, VirtPtr envs, ProgramLoadCallback callback, void* udata);

void execveSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#endif
