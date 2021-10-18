
#include <stdint.h>
#include <stddef.h>

#include "interrupt/syscall.h"

#include "error/log.h"
#include "loader/loader.h"
#include "memory/syscall.h"
#include "process/syscall.h"
#include "files/syscall.h"
#include "memory/kalloc.h"

#define TABLE_SIZE 512

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
    }
}

char* copyStringFromSyscallArgs(Process* process, uintptr_t ptr) {
    VirtPtr str = virtPtrFor(ptr, process->memory.mem);
    size_t length = strlenVirtPtr(str);
    char* string = kalloc(length + 1);
    if (string != NULL) {
        memcpyBetweenVirtPtr(virtPtrForKernel(string), str, length);
        string[length] = 0;
    }
    return string;
}

