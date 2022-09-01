#ifndef _FILES_SYSCALL_H_
#define _FILES_SYSCALL_H_

#include "interrupt/syscall.h"

SyscallReturn openSyscall(TrapFrame* frame);

SyscallReturn linkSyscall(TrapFrame* frame);

SyscallReturn unlinkSyscall(TrapFrame* frame);

SyscallReturn renameSyscall(TrapFrame* frame);

SyscallReturn closeSyscall(TrapFrame* frame);

SyscallReturn readSyscall(TrapFrame* frame);

SyscallReturn writeSyscall(TrapFrame* frame);

SyscallReturn seekSyscall(TrapFrame* frame);

SyscallReturn statSyscall(TrapFrame* frame);

SyscallReturn dupSyscall(TrapFrame* frame);

SyscallReturn truncSyscall(TrapFrame* frame);

SyscallReturn chmodSyscall(TrapFrame* frame);

SyscallReturn chownSyscall(TrapFrame* frame);

SyscallReturn mountSyscall(TrapFrame* frame);

SyscallReturn umountSyscall(TrapFrame* frame);

SyscallReturn readdirSyscall(TrapFrame* frame);

SyscallReturn chdirSyscall(TrapFrame* frame);

SyscallReturn getcwdSyscall(TrapFrame* frame);

SyscallReturn pipeSyscall(TrapFrame* frame);

SyscallReturn mknodSyscall(TrapFrame* frame);

SyscallReturn umaskSyscall(TrapFrame* frame);

SyscallReturn fcntlSyscall(TrapFrame* frame);

SyscallReturn ioctlSyscall(TrapFrame* frame);

SyscallReturn isattySyscall(TrapFrame* frame);

#endif
