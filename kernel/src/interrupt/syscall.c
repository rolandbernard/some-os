
#include <stdint.h>
#include <stddef.h>

#include "interrupt/syscall.h"

#include "error/log.h"
#include "loader/loader.h"
#include "memory/syscall.h"
#include "process/syscall.h"
#include "task/syscall.h"
#include "files/syscall.h"
#include "memory/kalloc.h"
#include "kernel/syscall.h"

static SyscallFunction findSyscall(Syscalls sys) {
    switch (sys) {
        case SYSCALL_PRINT: return printSyscall;
        case SYSCALL_EXIT: return exitSyscall;
        case SYSCALL_YIELD: return yieldSyscall;
        case SYSCALL_FORK: return forkSyscall;
        case SYSCALL_SLEEP: return sleepSyscall;
        case SYSCALL_OPEN: return openSyscall;
        case SYSCALL_LINK: return linkSyscall;
        case SYSCALL_UNLINK: return unlinkSyscall;
        case SYSCALL_RENAME: return renameSyscall;
        case SYSCALL_CLOSE: return closeSyscall;
        case SYSCALL_READ: return readSyscall;
        case SYSCALL_WRITE: return writeSyscall;
        case SYSCALL_SEEK: return seekSyscall;
        case SYSCALL_STAT: return statSyscall;
        case SYSCALL_DUP: return dupSyscall;
        case SYSCALL_TRUNC: return truncSyscall;
        case SYSCALL_CHMOD: return chmodSyscall;
        case SYSCALL_CHOWN: return chownSyscall;
        case SYSCALL_MOUNT: return mountSyscall;
        case SYSCALL_UMOUNT: return umountSyscall;
        case SYSCALL_EXECVE: return execveSyscall;
        case SYSCALL_READDIR: return readdirSyscall;
        case SYSCALL_GETPID: return getpidSyscall;
        case SYSCALL_GETPPID: return getppidSyscall;
        case SYSCALL_WAIT: return waitSyscall;
        case SYSCALL_SBRK: return sbrkSyscall;
        case SYSCALL_PROTECT: return protectSyscall;
        case SYSCALL_SIGACTION: return sigactionSyscall;
        case SYSCALL_SIGRETURN: return sigreturnSyscall;
        case SYSCALL_KILL: return killSyscall;
        case SYSCALL_GETUID: return getUidSyscall;
        case SYSCALL_GETGID: return getGidSyscall;
        case SYSCALL_SETUID: return setUidSyscall;
        case SYSCALL_SETGID: return setGidSyscall;
        case SYSCALL_CHDIR: return chdirSyscall;
        case SYSCALL_GETCWD: return getcwdSyscall;
        case SYSCALL_PIPE: return pipeSyscall;
        case SYSCALL_TIMES: return timesSyscall;
        case SYSCALL_PAUSE: return pauseSyscall;
        case SYSCALL_ALARM: return alarmSyscall;
        case SYSCALL_SIGPENDING: return sigpendingSyscall;
        case SYSCALL_SIGPROCMASK: return sigprocmaskSyscall;
        case SYSCALL_MKNOD: return mknodSyscall;
        case SYSCALL_CRITICAL: return criticalSyscall;
        default: return NULL;
    }
}

void runSyscall(TrapFrame* frame, bool is_kernel) {
    frame->pc += 4;
    Syscalls kind = (uintptr_t)frame->regs[REG_ARGUMENT_0];
    SyscallFunction func = findSyscall(kind);
    if (func != NULL && (is_kernel || kind < KERNEL_ONLY_SYSCALL_OFFSET)) {
        func(is_kernel, frame, &(frame->regs[REG_ARGUMENT_1]));
    } else {
        frame->regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

char* copyStringFromSyscallArgs(Task* task, uintptr_t ptr) {
    VirtPtr str = virtPtrForTask(ptr, task);
    size_t length = strlenVirtPtr(str);
    char* string = kalloc(length + 1);
    if (string != NULL) {
        memcpyBetweenVirtPtr(virtPtrForKernel(string), str, length);
        string[length] = 0;
    }
    return string;
}

