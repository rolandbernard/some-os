
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

#define TABLE_SIZE 128

SyscallFunction syscall_table[TABLE_SIZE] = {
    [SYSCALL_PRINT] = printSyscall,
    [SYSCALL_EXIT] = exitSyscall,
    [SYSCALL_YIELD] = yieldSyscall,
    [SYSCALL_FORK] = forkSyscall,
    [SYSCALL_SLEEP] = sleepSyscall,
    [SYSCALL_OPEN] = openSyscall,
    [SYSCALL_LINK] = linkSyscall,
    [SYSCALL_UNLINK] = unlinkSyscall,
    [SYSCALL_RENAME] = renameSyscall,
    [SYSCALL_CLOSE] = closeSyscall,
    [SYSCALL_READ] = readSyscall,
    [SYSCALL_WRITE] = writeSyscall,
    [SYSCALL_SEEK] = seekSyscall,
    [SYSCALL_STAT] = statSyscall,
    [SYSCALL_DUP] = dupSyscall,
    [SYSCALL_TRUNC] = truncSyscall,
    [SYSCALL_CHMOD] = chmodSyscall,
    [SYSCALL_CHOWN] = chownSyscall,
    [SYSCALL_MOUNT] = mountSyscall,
    [SYSCALL_UMOUNT] = umountSyscall,
    [SYSCALL_EXECVE] = execveSyscall,
    [SYSCALL_READDIR] = readdirSyscall,
    [SYSCALL_GETPID] = getpidSyscall,
    [SYSCALL_GETPPID] = getppidSyscall,
    [SYSCALL_WAIT] = waitSyscall,
    [SYSCALL_SBRK] = sbrkSyscall,
    [SYSCALL_PROTECT] = protectSyscall,
    [SYSCALL_SIGACTION] = sigactionSyscall,
    [SYSCALL_SIGRETURN] = sigreturnSyscall,
    [SYSCALL_KILL] = killSyscall,
    [SYSCALL_GETUID] = getUidSyscall,
    [SYSCALL_GETGID] = getGidSyscall,
    [SYSCALL_SETUID] = setUidSyscall,
    [SYSCALL_SETGID] = setGidSyscall,
    [SYSCALL_CHDIR] = chdirSyscall,
    [SYSCALL_GETCWD] = getcwdSyscall,
    [SYSCALL_PIPE] = pipeSyscall,
    [SYSCALL_TIMES] = timesSyscall,
    [SYSCALL_PAUSE] = pauseSyscall,
    [SYSCALL_ALARM] = alarmSyscall,
    [SYSCALL_SIGPENDING] = sigpendingSyscall,
    [SYSCALL_SIGPROCMASK] = sigprocmaskSyscall,
    [SYSCALL_MKNOD] = mknodSyscall,
};

void registerSyscall(int kind, SyscallFunction function) {
    if (kind < TABLE_SIZE) {
        syscall_table[kind] = function;
    }
}

void runSyscall(TrapFrame* frame, bool is_kernel) {
    frame->pc += 4;
    Syscalls kind = (uintptr_t)frame->regs[REG_ARGUMENT_0];
    if (kind < TABLE_SIZE && syscall_table[kind] != NULL) {
        syscall_table[kind](is_kernel, frame, &(frame->regs[REG_ARGUMENT_1]));
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

