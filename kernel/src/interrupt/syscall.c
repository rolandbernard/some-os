
#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "interrupt/syscall.h"

#include "error/log.h"
#include "files/syscall.h"
#include "kernel/syscall.h"
#include "loader/loader.h"
#include "memory/kalloc.h"
#include "memory/syscall.h"
#include "process/syscall.h"
#include "task/schedule.h"
#include "task/syscall.h"

SyscallFunction user_syscalls[] = {
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

SyscallFunction kernel_syscalls[] = {
    [SYSCALL_CRITICAL] = criticalSyscall,
};

static SyscallFunction findSyscall(Syscalls id) {
}

static bool isSyncSyscall(Syscalls id) {
    return id == SYSCALL_CRITICAL;
}

static void invokeSyscall(SyscallFunction func, TrapFrame* frame) {
    frame->regs[REG_ARGUMENT_0] = func(frame, &(frame->regs[REG_ARGUMENT_1]));
}

static void syscallTaskEntry(SyscallFunction func, TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    invokeSyscall(func, frame);
    task->sched.state = ENQUABLE;
    enqueueTask(task);
    leave();
}

void runSyscall(TrapFrame* frame, bool is_kernel) {
    frame->pc += 4;
    Syscalls kind = (uintptr_t)frame->regs[REG_ARGUMENT_0];
    SyscallFunction func = findSyscall(kind);
    if (func != NULL && (is_kernel || kind < KERNEL_ONLY_SYSCALL_OFFSET)) {
        if (isSyncSyscall(kind)) {
            invokeSyscall(func, frame);
        } else {
            assert(frame->hart != NULL);
            Task* task = (Task*)frame;
            task->sched.state = WAITING;
            
        }
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

