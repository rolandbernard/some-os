
#include <assert.h>
#include <string.h>

#include "files/syscall.h"

#include "files/path.h"
#include "files/process.h"
#include "files/special/pipe.h"
#include "files/vfs/file.h"
#include "files/vfs/fs.h"
#include "files/vfs/super.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "task/schedule.h"
#include "task/types.h"
#include "util/util.h"

VfsOpenFlags convertOpenMode(VfsOpenFlags arg) {
    VfsOpenFlags flags = arg;
    VfsOpenFlags access_mode = (flags + 1) & VFS_OPEN_ACCESS_MODE;
    return (flags & ~VFS_OPEN_ACCESS_MODE) | access_mode;
}

SyscallReturn openSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (string != NULL) {
        VfsFile* file;
        VfsOpenFlags flags = convertOpenMode(SYSCALL_ARG(1));
        Error err = vfsOpenAt(&global_file_system, task->process, NULL, string, flags, SYSCALL_ARG(2), &file);
        dealloc(string);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            file->flags = flags & (VFS_OPEN_ACCESS_MODE | VFS_FILE_NONBLOCK);
            int fd = putNewFileDescriptor(
                task->process, -1, (flags & VFS_OPEN_CLOEXEC) != 0 ? VFS_DESC_CLOEXEC : 0, file, false
            );
            vfsFileClose(file);
            SYSCALL_RETURN(fd);
        }
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn unlinkSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (string != NULL) {
        Error err = vfsUnlinkAt(&global_file_system, task->process, NULL, string);
        dealloc(string);
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn linkSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* old = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (old != NULL) {
        char* new = copyStringFromSyscallArgs(task, SYSCALL_ARG(1));
        if (new != NULL) {
            Error err = vfsLinkAt(&global_file_system, task->process, NULL, old, NULL, new);
            dealloc(new);
            dealloc(old);
            SYSCALL_RETURN(-err.kind);
        } else {
            dealloc(old);
            SYSCALL_RETURN(-EINVAL);
        }
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn renameSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* old = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (old != NULL) {
        char* new = copyStringFromSyscallArgs(task, SYSCALL_ARG(1));
        if (new != NULL) {
            Error err = vfsLinkAt(&global_file_system, task->process, NULL, old, NULL, new);
            if (!isError(err)) {
                err = vfsUnlinkAt(&global_file_system, task->process, NULL, old);
            }
            dealloc(new);
            dealloc(old);
            SYSCALL_RETURN(-err.kind);
        } else {
            dealloc(old);
            SYSCALL_RETURN(-EINVAL);
        }
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

#define FILE_SYSCALL_OP(READ, WRITE)                                    \
    assert(frame->hart != NULL);                                        \
    Task* task = (Task*)frame;                                          \
    assert(task->process != NULL);                                      \
    size_t fd = SYSCALL_ARG(0);                                         \
    VfsFileDescriptor* desc = getFileDescriptor(task->process, fd);     \
    if (desc != NULL) {                                                 \
        if ((desc->file->flags & VFS_FILE_WRITE) == 0 && WRITE) {       \
            vfsFileDescriptorClose(task->process, desc);                \
            SYSCALL_RETURN(-EBADF);                                     \
        } else if ((desc->file->flags & VFS_FILE_READ) == 0 && READ) {  \
            vfsFileDescriptorClose(task->process, desc);                \
            SYSCALL_RETURN(-EBADF);                                     \
        }                                                               \
    } else {                                                            \
        SYSCALL_RETURN(-EBADF);                                         \
    }

SyscallReturn closeSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false);
    closeFileDescriptor(task->process, fd);
    vfsFileDescriptorClose(task->process, desc);
    SYSCALL_RETURN(-SUCCESS);
}

SyscallReturn readSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(true, false);
    size_t size;
    Error err = vfsFileRead(desc->file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size);
    vfsFileDescriptorClose(task->process, desc);
    if (isError(err)) {
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(size);
    }
}

SyscallReturn writeSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true);
    size_t size;
    Error err = vfsFileWrite(desc->file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size);
    vfsFileDescriptorClose(task->process, desc);
    if (isError(err)) {
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(size);
    }
}

SyscallReturn seekSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false);
    size_t size;
    Error err = vfsFileSeek(desc->file, task->process, SYSCALL_ARG(1), SYSCALL_ARG(2), &size);
    vfsFileDescriptorClose(task->process, desc);
    if (isError(err)) {
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(size);
    }
}

SyscallReturn statSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false);
    VirtPtr ptr = virtPtrForTask(SYSCALL_ARG(1), task);
    Error err = vfsFileStat(desc->file, task->process, ptr);
    vfsFileDescriptorClose(task->process, desc);
    SYSCALL_RETURN(-err.kind);
}

SyscallReturn dupSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false);
    int flags = desc->flags & ~VFS_DESC_CLOEXEC;
    if ((SYSCALL_ARG(2) & VFS_OPEN_CLOEXEC) != 0) {
        flags |= VFS_DESC_CLOEXEC;
    }
    int id = SYSCALL_ARG(1);
    id = putNewFileDescriptor(task->process, id, flags, desc->file, true);
    vfsFileDescriptorClose(task->process, desc);
    SYSCALL_RETURN(id);
}

SyscallReturn truncSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true);
    Error err = vfsFileTrunc(desc->file, task->process, SYSCALL_ARG(1));
    vfsFileDescriptorClose(task->process, desc);
    SYSCALL_RETURN(-err.kind);
}

SyscallReturn chmodSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true);
    Error err = vfsFileChmod(desc->file, task->process, SYSCALL_ARG(1));
    vfsFileDescriptorClose(task->process, desc);
    SYSCALL_RETURN(-err.kind);
}

SyscallReturn chownSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true);
    Error err = vfsFileChown(desc->file, task->process, SYSCALL_ARG(1), SYSCALL_ARG(2));
    vfsFileDescriptorClose(task->process, desc);
    SYSCALL_RETURN(-err.kind);
}

SyscallReturn readdirSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(true, false);
    size_t size;
    Error err = vfsFileReaddir(
        desc->file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size
    );
    vfsFileDescriptorClose(task->process, desc);
    if (isError(err)) {
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(size);
    }
}

SyscallReturn mountSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process != NULL) {
        lockSpinLock(&task->process->user.lock);
        if (task->process->user.euid != 0) {
            unlockSpinLock(&task->process->user.lock);
            // Only root can mount
            SYSCALL_RETURN(-EPERM);
        }
        unlockSpinLock(&task->process->user.lock);
    }
    char* source = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (source != NULL) {
        char* type = copyStringFromSyscallArgs(task, SYSCALL_ARG(2));
        if (type != NULL) {
            VfsSuperblock* sb;
            Error err = vfsCreateSuperblock(
                &global_file_system, task->process, source, type,
                virtPtrForTask(SYSCALL_ARG(3), task), &sb
            );
            dealloc(type);
            dealloc(source);
            if (isError(err)) {
                SYSCALL_RETURN(-err.kind);
            } else {
                char* target = copyStringFromSyscallArgs(task, SYSCALL_ARG(1));
                if (target != NULL) {
                    err = vfsMount(&global_file_system, task->process, target, sb);
                    dealloc(target);
                    if (isError(err)) {
                        vfsSuperClose(sb);
                    }
                    SYSCALL_RETURN(-err.kind);
                } else {
                    vfsSuperClose(sb);
                    SYSCALL_RETURN(-EINVAL);
                }
            }
        } else {
            dealloc(source);
            SYSCALL_RETURN(-EINVAL);
        }
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn umountSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process != NULL) {
        lockSpinLock(&task->process->user.lock);
        if (task->process->user.euid != 0) {
            unlockSpinLock(&task->process->user.lock);
            // Only root can unmount
            SYSCALL_RETURN(-EPERM);
        }
        unlockSpinLock(&task->process->user.lock);
    }
    char* path = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (path != NULL) {
        Error err = vfsUmount(&global_file_system, task->process, path);
        dealloc(path);
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn chdirSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    char* path = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (path != NULL) {
        VfsFile* file;
        Error err = vfsOpenAt(&global_file_system, task->process, NULL, path, VFS_OPEN_DIRECTORY, 0, &file);
        dealloc(path);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        }
        lockTaskLock(&task->process->resources.lock);
        dealloc(task->process->resources.cwd);
        task->process->resources.cwd = stringClone(file->path);
        unlockTaskLock(&task->process->resources.lock);
        vfsFileClose(file);
        SYSCALL_RETURN(-SUCCESS);
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn getcwdSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    VirtPtr buff = virtPtrForTask(SYSCALL_ARG(0), task);
    size_t length = SYSCALL_ARG(1);
    lockTaskLock(&task->process->resources.lock);
    size_t cwd_length = strlen(task->process->resources.cwd);
    memcpyBetweenVirtPtr(
        buff, virtPtrForKernel(task->process->resources.cwd), umin(length, cwd_length + 1)
    );
    unlockTaskLock(&task->process->resources.lock);
    SYSCALL_RETURN(-SUCCESS);
}

SyscallReturn pipeSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    VfsFile* file_read = createPipeFile(false);
    VfsFile* file_write = createPipeFileClone(file_read, true);
    file_read->flags = VFS_FILE_READ;
    file_write->flags = VFS_FILE_WRITE;
    int pipe_read = putNewFileDescriptor(task->process, -1, 0, (VfsFile*)file_read, false);
    int pipe_write = putNewFileDescriptor(task->process, -1, 0, (VfsFile*)file_write, false);
    vfsFileClose(file_read);
    vfsFileClose(file_write);
    VirtPtr arr = virtPtrForTask(SYSCALL_ARG(0), task);
    writeIntAt(arr, sizeof(int) * 8, 0, pipe_read);
    writeIntAt(arr, sizeof(int) * 8, 1, pipe_write);
    SYSCALL_RETURN(-SUCCESS);
}

SyscallReturn mknodSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (string != NULL) {
        Error err = vfsMknodAt(&global_file_system, task->process, NULL, string, SYSCALL_ARG(1), SYSCALL_ARG(2));
        dealloc(string);
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn umaskSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    VfsMode old_mask;
    lockTaskLock(&task->process->resources.lock);
    old_mask = task->process->resources.umask;
    task->process->resources.umask = SYSCALL_ARG(0) & 0777;
    unlockTaskLock(&task->process->resources.lock);
    SYSCALL_RETURN(old_mask);
}

SyscallReturn fcntlSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false);
    VfsFcntlRequest request = SYSCALL_ARG(1);
    switch (request) {
        case VFS_FCNTL_DUPFD:
        case VFS_FCNTL_DUPFD_CLOEXEC: {
            int id = SYSCALL_ARG(2);
            int flags = request == VFS_FCNTL_DUPFD_CLOEXEC ? VFS_DESC_CLOEXEC : 0;
            id = putNewFileDescriptor(task->process, id, flags, desc->file, false);
            vfsFileDescriptorClose(task->process, desc);
            SYSCALL_RETURN(id);
        }
        case VFS_FCNTL_GETFD: {
            int flags = desc->flags;
            vfsFileDescriptorClose(task->process, desc);
            SYSCALL_RETURN(flags);
        }
        case VFS_FCNTL_SETFD: {
            desc->flags = SYSCALL_ARG(2);
            vfsFileDescriptorClose(task->process, desc);
            SYSCALL_RETURN(0);
        }
        case VFS_FCNTL_GETFL: {
            int flags = desc->file->flags;
            vfsFileDescriptorClose(task->process, desc);
            SYSCALL_RETURN(flags);
        }
        case VFS_FCNTL_SETFL: {
            // Access mode flags must be preserved.
            desc->file->flags = (SYSCALL_ARG(2) & VFS_FILE_NONBLOCK)
                | (desc->file->flags & VFS_FILE_ACCESS);
            vfsFileDescriptorClose(task->process, desc);
            SYSCALL_RETURN(0);
        }
        default:
            SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn ioctlSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false);
    uintptr_t result = 0;
    Error error = vfsFileIoctl(desc->file, task->process, SYSCALL_ARG(1), virtPtrForTask(SYSCALL_ARG(2), task), &result);
    vfsFileDescriptorClose(task->process, desc);
    if (isError(error)) {
        SYSCALL_RETURN(-error.kind);
    } else {
        SYSCALL_RETURN(result);
    }
}

SyscallReturn isattySyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false);
    lockTaskLock(&desc->file->node->lock);
    if (MODE_TYPE(desc->file->node->stat.mode) == VFS_TYPE_CHAR) {
        Device* dev = getDeviceWithId(desc->file->node->stat.rdev);
        unlockTaskLock(&desc->file->node->lock);
        vfsFileDescriptorClose(task->process, desc);
        if (dev != NULL && strcmp(dev->name, "tty") == 0) {
            SYSCALL_RETURN(-SUCCESS);
        } else {
            SYSCALL_RETURN(-ENOTTY);
        }
    } else {
        unlockTaskLock(&desc->file->node->lock);
        vfsFileDescriptorClose(task->process, desc);
        SYSCALL_RETURN(-ENOTTY);
    }
}

static bool handleSelectWakeup(Task* task, void* _) {
    TrapFrame* frame = (TrapFrame*)task;
    if (task->process != NULL) {
        lockSpinLock(&task->process->lock); 
        if (task->process->signals.signals != NULL) {
            unlockSpinLock(&task->process->lock); 
            task->frame.regs[REG_ARGUMENT_0] = -EINTR;
            return true;
        } else {
            unlockSpinLock(&task->process->lock); 
        }
    }
    size_t num_ready = 0;
    size_t num_fds = SYSCALL_ARG(0);
    VirtPtr reads_ptr = virtPtrForTask(SYSCALL_ARG(1), task);
    VirtPtr writes_ptr = virtPtrForTask(SYSCALL_ARG(2), task);
    VirtPtr excepts_ptr = virtPtrForTask(SYSCALL_ARG(3), task);
    uint64_t reads = readInt(reads_ptr, 64);
    uint64_t writes = readInt(writes_ptr, 64);
    for (size_t i = 0; i < num_fds; i++) {
        if ((reads & (1UL << i)) != 0) {
            // TODO: This is safe only as long as we have only one task per process
            VfsFileDescriptor* desc = getFileDescriptorUnsafe(task->process, i);
            if (desc == NULL || (desc->file->flags & VFS_FILE_READ) == 0) {
                task->frame.regs[REG_ARGUMENT_0] = -EBADF;
                return true;
            } else {
                if (!vfsFileWillBlock(desc->file, task->process, false)) {
                    num_ready++;
                } else {
                    reads &= ~(1UL << i);
                }
            }
        }
        if ((writes & (1 << i)) != 0) {
            VfsFileDescriptor* desc = getFileDescriptorUnsafe(task->process, i);
            if (desc == NULL || (desc->file->flags & VFS_FILE_WRITE) == 0) {
                task->frame.regs[REG_ARGUMENT_0] = -EBADF;
                return true;
            } else {
                if (!vfsFileWillBlock(desc->file, task->process, true)) {
                    num_ready++;
                } else {
                    writes &= ~(1UL << i);
                }
            }
        }
    }
    if (num_ready != 0) {
        writeInt(reads_ptr, 64, reads);
        writeInt(writes_ptr, 64, writes);
        writeInt(excepts_ptr, 64, 0);
        task->frame.regs[REG_ARGUMENT_0] = num_ready;
        return true;
    }
    Time timeout = SYSCALL_ARG(4);
    if (timeout != (Time)-1 && getTime() >= task->times.entered + (timeout / (1000000000UL / CLOCKS_PER_SEC))) {
        writeInt(reads_ptr, 64, 0);
        writeInt(writes_ptr, 64, 0);
        writeInt(excepts_ptr, 64, 0);
        task->frame.regs[REG_ARGUMENT_0] = 0;
        return true;
    }
    return false;
}

SyscallReturn selectSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    assert(task->process != NULL);
    Time timeout = SYSCALL_ARG(4);
    lockSpinLock(&task->sched.lock); 
    task->times.entered = getTime();
    task->sched.wakeup_function = handleSelectWakeup;
    unlockSpinLock(&task->sched.lock); 
    if (timeout != (Time)-1) {
        // Make sure we wake up in time
        setTimeoutTime(getTime() + timeout / (1000000000UL / CLOCKS_PER_SEC), NULL, NULL);
    }
    return WAIT;
}

