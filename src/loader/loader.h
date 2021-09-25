#ifndef _LOADER_H_
#define _LOADER_H_

#include "process/types.h"
#include "interrupt/syscall.h"
#include "memory/virtptr.h"
#include "files/vfs.h"

uintptr_t execveSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args);

#endif
