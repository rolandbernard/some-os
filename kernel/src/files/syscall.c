
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
            file->flags = flags & VFS_OPEN_ACCESS_MODE;
            int fd = putNewFileDescriptor(task->process, -1, (flags & VFS_OPEN_CLOEXEC) != 0 ? VFS_DESC_CLOEXEC : 0, file);
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

#define FILE_SYSCALL_OP(READ, WRITE, DO)                                \
    assert(frame->hart != NULL);                                        \
    Task* task = (Task*)frame;                                          \
    assert(task->process != NULL);                                      \
    size_t fd = SYSCALL_ARG(0);                                         \
    VfsFileDescriptor* desc = getFileDescriptor(task->process, fd);     \
    if (desc != NULL) {                                                 \
        if ((desc->file->flags & VFS_FILE_WRITE) == 0 && WRITE) {       \
            SYSCALL_RETURN(-EBADF);                                     \
        } else if ((desc->file->flags & VFS_FILE_READ) == 0 && READ) {  \
            SYSCALL_RETURN(-EBADF);                                     \
        } else {                                                        \
            DO;                                                         \
        }                                                               \
    } else {                                                            \
        SYSCALL_RETURN(-EBADF);                                         \
    }

SyscallReturn closeSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, {
        closeFileDescriptor(task->process, fd);
        vfsFileDescriptorClose(task->process, desc);
        SYSCALL_RETURN(-SUCCESS);
    });
}

SyscallReturn readSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(true, false, {
        size_t size;
        Error err = vfsFileRead(desc->file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size);
        vfsFileDescriptorClose(task->process, desc);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            SYSCALL_RETURN(size);
        }
    });
}

SyscallReturn writeSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, {
        size_t size;
        Error err = vfsFileWrite(desc->file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size);
        vfsFileDescriptorClose(task->process, desc);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            SYSCALL_RETURN(size);
        }
    });
}

SyscallReturn seekSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, {
        size_t size;
        Error err = vfsFileSeek(desc->file, task->process, SYSCALL_ARG(1), SYSCALL_ARG(2), &size);
        vfsFileDescriptorClose(task->process, desc);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            SYSCALL_RETURN(size);
        }
    });
}

SyscallReturn statSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, {
        VirtPtr ptr = virtPtrForTask(SYSCALL_ARG(1), task);
        Error err = vfsFileStat(desc->file, task->process, ptr);
        vfsFileDescriptorClose(task->process, desc);
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn dupSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, {
        int flags = desc->flags & ~VFS_DESC_CLOEXEC;
        if ((SYSCALL_ARG(2) & VFS_OPEN_CLOEXEC) != 0) {
            flags |= VFS_DESC_CLOEXEC;
        }
        int id = SYSCALL_ARG(1);
        id = putNewFileDescriptor(task->process, id, flags, desc->file);
        vfsFileDescriptorClose(task->process, desc);
        SYSCALL_RETURN(id);
    });
}

SyscallReturn truncSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, {
        Error err = vfsFileTrunc(desc->file, task->process, SYSCALL_ARG(1));
        vfsFileDescriptorClose(task->process, desc);
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn chmodSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, {
        Error err = vfsFileChmod(desc->file, task->process, SYSCALL_ARG(1));
        vfsFileDescriptorClose(task->process, desc);
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn chownSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, {
        Error err = vfsFileChown(desc->file, task->process, SYSCALL_ARG(1), SYSCALL_ARG(2));
        vfsFileDescriptorClose(task->process, desc);
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn readdirSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(true, false, {
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
    });
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
    VfsFile* file_read = createPipeFile();
    VfsFile* file_write = createPipeFileClone(file_read);
    file_read->flags = VFS_FILE_READ;
    file_write->flags = VFS_FILE_WRITE;
    int pipe_read = putNewFileDescriptor(task->process, -1, 0, (VfsFile*)file_read);
    int pipe_write = putNewFileDescriptor(task->process, -1, 0, (VfsFile*)file_write);
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
    SYSCALL_RETURN(-ENOSYS);
}

SyscallReturn ioctlSyscall(TrapFrame* frame) {
    SYSCALL_RETURN(-ENOSYS);
}

SyscallReturn isattySyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, {
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
    });
}

