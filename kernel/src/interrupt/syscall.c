
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
#include "task/harts.h"
#include "task/schedule.h"
#include "task/syscall.h"
#include "util/util.h"

#define SYSCALL_STACK_SIZE HART_STACK_SIZE

SyscallFunction user_syscalls[] = {
    [SYSCALL_UNKNOWN] = NULL,
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
    [SYSCALL_UMASK] = umaskSyscall,
    [SYSCALL_GETEUID] = getEUidSyscall,
    [SYSCALL_GETEGID] = getEGidSyscall,
    [SYSCALL_SETEUID] = setEUidSyscall,
    [SYSCALL_SETEGID] = setEGidSyscall,
    [SYSCALL_SETREUID] = setREUidSyscall,
    [SYSCALL_SETREGID] = setREGidSyscall,
    [SYSCALL_FCNTL] = fcntlSyscall,
    [SYSCALL_IOCTL] = ioctlSyscall,
    [SYSCALL_ISATTY] = isattySyscall,
    [SYSCALL_GET_NANOSECONDS] = getNanosecondsSyscall,
    [SYSCALL_SETPGID] = setPgidSyscall,
    [SYSCALL_GETPGID] = getPgidSyscall,
    [SYSCALL_SETSID] = setSidSyscall,
    [SYSCALL_GETSID] = getSidSyscall,
    [SYSCALL_SET_NANOSECONDS] = setNanosecondsSyscall,
    [SYSCALL_SELECT] = selectSyscall,
};

SyscallFunction kernel_syscalls[] = {
    [SYSCALL_CRITICAL - KERNEL_ONLY_SYSCALL_OFFSET] = criticalSyscall,
};

static SyscallFunction findSyscall(Syscalls id) {
    if (id < KERNEL_ONLY_SYSCALL_OFFSET) {
        if (id < ARRAY_LENGTH(user_syscalls)) {
            return user_syscalls[id];
        } else {
            return NULL;
        }
    } else {
        id -= KERNEL_ONLY_SYSCALL_OFFSET;
        if (id < ARRAY_LENGTH(kernel_syscalls)) {
            return kernel_syscalls[id];
        } else {
            return NULL;
        }
    }
}

#ifdef DEBUG_LOG_SYSCALLS
#include "error/debuginfo.h"

static const char* findSyscallName(Syscalls id) {
    SyscallFunction func = findSyscall(id);
    if (func == NULL) {
        return NULL;
    } else {
        const SymbolDebugInfo* symb = searchSymbolDebugInfo((uintptr_t)func);
        return symb == NULL ? NULL : symb->symbol;
    }
}
#endif

static bool isSyncSyscall(Syscalls id) {
    return id == SYSCALL_CRITICAL;
}

static void syscallTaskEnd(void* _, Task* task) {
    moveTaskToState(task, TERMINATED);
    enqueueTask(task);
    runNextTask();
}

static void syscallTask(SyscallFunction func, TrapFrame* frame) {
    assert(getCurrentTask() != NULL);
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    SyscallReturn ret = func(frame);
    Task* self = criticalEnter();
    assert(self != NULL); // Make sure we did not miss to call criticalReturn somewhere
    task->times.system_time += self->times.user_time + self->times.system_time;
    task->times.system_time += self->times.user_child_time + self->times.system_child_time;
    if (ret == CONTINUE) {
        awakenTask(task);
        enqueueTask(task);
    }
    callInHart((void*)syscallTaskEnd, self);
}

void runSyscall(TrapFrame* frame, bool is_kernel) {
    frame->pc += 4;
    Syscalls kind = (uintptr_t)frame->regs[REG_ARGUMENT_0];
#ifdef DEBUG_LOG_SYSCALLS
    if (kind != SYSCALL_CRITICAL) {
        const char* name = findSyscallName(kind);
        if (frame->hart == NULL) {
            HartFrame* hart = (HartFrame*)frame;
            KERNEL_DEBUG("%s from hart %i (%p)", name, hart->hartid, frame);
        } else {
            Task* task = (Task*)frame;
            if (task->process != NULL) {
                KERNEL_DEBUG("%s from user process %i (%p)", name, task->process->pid, frame);
            } else {
                KERNEL_DEBUG("%s from kernel task (%p)", name, frame);
            }
        }
    }
#endif
    SyscallFunction func = findSyscall(kind);
    if (func != NULL && (is_kernel || kind < KERNEL_ONLY_SYSCALL_OFFSET)) {
        if (isSyncSyscall(kind)) {
            func(frame);
        } else {
            assert(frame->hart != NULL); // Only tasks can wait for async syscalls
            Task* task = (Task*)frame;
            task->sched.wakeup_function = NULL;
            moveTaskToState(task, WAITING);
            task->sys_task = createKernelTask(syscallTask, SYSCALL_STACK_SIZE, task->sched.priority);
            task->sys_task->frame.regs[REG_ARGUMENT_0] = (uintptr_t)func;
            task->sys_task->frame.regs[REG_ARGUMENT_1] = (uintptr_t)frame;
            enqueueTask(task->sys_task);
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

