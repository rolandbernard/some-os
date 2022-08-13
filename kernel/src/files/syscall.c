
#include <assert.h>
#include <string.h>

#include "files/syscall.h"

#include "files/path.h"
#include "files/process.h"
#include "files/special/pipe.h"
#include "files/vfs.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "task/schedule.h"
#include "task/types.h"
#include "util/util.h"

SyscallReturn openSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
    if (string != NULL) {
        VfsFile* file;
        Error err = vfsOpen(&global_file_system, task->process, string, SYSCALL_ARG(1), SYSCALL_ARG(2), &file);
        dealloc(string);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            size_t fd = allocateNewFileDescriptorId(task->process);
            int flags = 0;
            if ((SYSCALL_ARG(1) & VFS_OPEN_CLOEXEC) != 0) {
                flags |= VFS_FILE_CLOEXEC;
            }
            if ((SYSCALL_ARG(1) & VFS_OPEN_RDONLY) != 0) {
                flags |= VFS_FILE_RDONLY;
            }
            if ((SYSCALL_ARG(1) & VFS_OPEN_WRONLY) != 0) {
                flags |= VFS_FILE_WRONLY;
            }
            putNewFileDescriptor(task->process, fd, flags, file);
            SYSCALL_RETURN(fd);
        }
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn unlinkSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
    if (string != NULL) {
        Error err = vfsUnlink(&global_file_system, task->process, string);
        dealloc(string);
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn linkSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* old = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
    if (old != NULL) {
        char* new = copyPathFromSyscallArgs(task, SYSCALL_ARG(1));
        if (new != NULL) {
            Error err = vfsLink(&global_file_system, task->process, old, new);
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
    char* old = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
    if (old != NULL) {
        char* new = copyPathFromSyscallArgs(task, SYSCALL_ARG(1));
        if (new != NULL) {
            Error err = vfsRename(&global_file_system, task->process, old, new);
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

#define FILE_SYSCALL_OP(READ, WRITE, NAME, DO)      \
    FILE_SYSCALL_OP_DESC(READ, WRITE,               \
        if (desc->file->functions->NAME != NULL) {  \
            VfsFile* file = desc->file;             \
            DO                                      \
        } else {                                    \
            SYSCALL_RETURN(-EINVAL);                \
        }                                           \
    )

#define FILE_SYSCALL_OP_DESC(READ, WRITE, DO)                       \
    assert(frame->hart != NULL);                                    \
    Task* task = (Task*)frame;                                      \
    size_t fd = SYSCALL_ARG(0);                                     \
    FileDescriptor* desc = getFileDescriptor(task->process, fd);    \
    if (desc != NULL) {                                             \
        if ((desc->flags & VFS_FILE_RDONLY) != 0 && WRITE) {        \
            SYSCALL_RETURN(-EPERM);                                 \
        } else if ((desc->flags & VFS_FILE_WRONLY) != 0 && READ) {  \
            SYSCALL_RETURN(-EPERM);                                 \
        } else {                                                    \
            DO;                                                     \
        }                                                           \
    } else {                                                        \
        SYSCALL_RETURN(-EBADF);                                     \
    }

SyscallReturn closeSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP_DESC(false, false, {
        closeFileDescriptor(task->process, fd);
        SYSCALL_RETURN(-SUCCESS);
    });
}

SyscallReturn readSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(true, false, read, {
        size_t size;
        Error err = file->functions->read(file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            SYSCALL_RETURN(size);
        }
    });
}

SyscallReturn writeSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, write, {
        size_t size;
        Error err = file->functions->write(file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            SYSCALL_RETURN(size);
        }
    });
}

SyscallReturn seekSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, seek, {
        size_t size;
        Error err = file->functions->seek(file, task->process, SYSCALL_ARG(1), SYSCALL_ARG(2), &size);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            SYSCALL_RETURN(size);
        }
    });
}

SyscallReturn statSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, stat, {
        VirtPtr ptr = virtPtrForTask(SYSCALL_ARG(1), task);
        Error err = file->functions->stat(file, task->process, ptr);
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn dupSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP_DESC(false, false, {
        int flags = desc->flags & ~VFS_FILE_CLOEXEC;
        if ((SYSCALL_ARG(2) & VFS_OPEN_CLOEXEC) != 0) {
            flags |= VFS_FILE_CLOEXEC;
        }
        int id = SYSCALL_ARG(1);
        if (id < 0) {
            id = allocateNewFileDescriptorId(task->process);
        } else {
            closeFileDescriptor(task->process, id);
        }
        putNewFileDescriptor(task->process, id, flags, desc->file);
        SYSCALL_RETURN(id);
    });
}

SyscallReturn truncSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, trunc, {
        Error err = file->functions->trunc(file, task->process, SYSCALL_ARG(1));
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn chmodSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, chmod, {
        Error err = file->functions->chmod(file, task->process, SYSCALL_ARG(1));
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn chownSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, true, chown, {
        Error err = file->functions->chown(file, task->process, SYSCALL_ARG(1), SYSCALL_ARG(2));
        SYSCALL_RETURN(-err.kind);
    });
}

SyscallReturn readdirSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(true, false, readdir, {
        size_t size;
        Error err = file->functions->readdir(
            file, task->process, virtPtrForTask(SYSCALL_ARG(1), task), SYSCALL_ARG(2), &size
        );
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
    if (
        task->process != NULL && task->process->resources.uid != 0
        && task->process->resources.gid != 0
    ) { // Only root can mount
        SYSCALL_RETURN(-EACCES);
    } else {
        char* source = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
        if (source != NULL) {
            char* type = copyStringFromSyscallArgs(task, SYSCALL_ARG(2));
            if (type != NULL) {
                VfsFilesystem* fs;
                Error err = createFilesystemFrom(&global_file_system, source, type, virtPtrForTask(SYSCALL_ARG(3), task), &fs);
                dealloc(type);
                dealloc(source);
                if (isError(err)) {
                    SYSCALL_RETURN(-err.kind);
                } else {
                    char* target = copyPathFromSyscallArgs(task, SYSCALL_ARG(1));
                    if (target != NULL) {
                        err = mountFilesystem(&global_file_system, fs, target);
                        dealloc(target);
                        if (isError(err)) {
                            fs->functions->free(fs);
                        }
                        SYSCALL_RETURN(-err.kind);
                    } else {
                        fs->functions->free(fs);
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
}

SyscallReturn umountSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process->resources.uid != 0 && task->process->resources.gid != 0) {
        // Only root can mount
        SYSCALL_RETURN(-EACCES);
    } else {
        char* path = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
        if (path != NULL) {
            Error err = umount(&global_file_system, path);
            dealloc(path);
            SYSCALL_RETURN(-err.kind);
        } else {
            SYSCALL_RETURN(-EINVAL);
        }
    }
}

SyscallReturn chdirSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* path = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
    if (path != NULL) {
        dealloc(task->process->resources.cwd);
        task->process->resources.cwd = path;
        SYSCALL_RETURN(-SUCCESS);
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

SyscallReturn getcwdSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    VirtPtr buff = virtPtrForTask(SYSCALL_ARG(0), task);
    size_t length = SYSCALL_ARG(1);
    size_t cwd_length = strlen(task->process->resources.cwd);
    memcpyBetweenVirtPtr(
        buff, virtPtrForKernel(task->process->resources.cwd), umin(length, cwd_length + 1)
    );
    SYSCALL_RETURN(-SUCCESS);
}

char* copyPathFromSyscallArgs(Task* task, uintptr_t ptr) {
    VirtPtr str = virtPtrForTask(ptr, task);
    bool relative = true;
    size_t cwd_length = 0;
    if (task->process != NULL && task->process->resources.cwd != NULL) {
        cwd_length = strlen(task->process->resources.cwd);
    }
    size_t length = strlenVirtPtr(str);
    if (readInt(str, 8) == '/') {
        relative = false;
    }
    char* string = kalloc(length + 1 + (relative ? cwd_length + 1 : 0));
    if (string != NULL) {
        if (relative) {
            if (task->process != NULL && task->process->resources.cwd != NULL) {
                memcpy(string, task->process->resources.cwd, cwd_length);
            }
            string[cwd_length] = '/';
            memcpyBetweenVirtPtr(virtPtrForKernel(string + cwd_length + 1), str, length);
            string[cwd_length + 1 + length] = 0;
        } else {
            memcpyBetweenVirtPtr(virtPtrForKernel(string), str, length);
            string[length] = 0;
        }
        // All paths to the functions have to be normalized and absolute
        inlineReducePath(string);
        return string;
    } else {
        return NULL;
    }
}

SyscallReturn pipeSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    PipeFile* file_read = createPipeFile();
    if (file_read != NULL) {
        PipeFile* file_write = duplicatePipeFile(file_read);
        if (file_write == NULL) {
            file_read->base.functions->free((VfsFile*)file_read);
            SYSCALL_RETURN(-ENOMEM);
        } else {
            int pipe_read = allocateNewFileDescriptorId(task->process);
            int pipe_write = allocateNewFileDescriptorId(task->process);
            putNewFileDescriptor(task->process, pipe_read, VFS_FILE_RDONLY, (VfsFile*)file_read);
            putNewFileDescriptor(task->process, pipe_write, VFS_FILE_WRONLY, (VfsFile*)file_write);
            VirtPtr arr = virtPtrForTask(SYSCALL_ARG(0), task);
            writeIntAt(arr, sizeof(int) * 8, 0, pipe_read);
            writeIntAt(arr, sizeof(int) * 8, 1, pipe_write);
            SYSCALL_RETURN(-SUCCESS);
        }
    } else {
        SYSCALL_RETURN(-ENOMEM);
    }
}

SyscallReturn mknodSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyPathFromSyscallArgs(task, SYSCALL_ARG(0));
    if (string != NULL) {
        Error err = vfsMknod(&global_file_system, task->process, string, SYSCALL_ARG(1), SYSCALL_ARG(2));
        dealloc(string);
        SYSCALL_RETURN(-err.kind);
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

