
#include <assert.h>
#include <string.h>

#include "files/syscall.h"

#include "files/vfs.h"
#include "files/path.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "task/schedule.h"
#include "task/types.h"
#include "util/util.h"
#include "files/special/pipe.h"

static int allocateNewFileDescriptor(Process* process) {
    int fd = process->resources.next_fd;
    process->resources.next_fd++;
    return fd;
}

static VfsFile* getFileDescriptor(Process* process, int fd) {
    VfsFile* current = process->resources.files;
    while (current != NULL) {
        if (current->fd == fd) {
            return current;
        } else {
            current = current->next;
        }
    }
    return NULL;
}

static void putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file) {
    file->fd = fd;
    file->flags = flags;
    file->next = process->resources.files;
    process->resources.files = file;
}

static VfsFile* removeFileDescriptor(Process* process, int fd) {
    VfsFile** current = &process->resources.files;
    while (*current != NULL) {
        if ((*current)->fd == fd) {
            VfsFile* to_remove = *current;
            *current = to_remove->next;
            return to_remove;
        } else {
            current = &(*current)->next;
        }
    }
    return NULL;
}

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
            size_t fd = allocateNewFileDescriptor(task->process);
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

#define FILE_SYSCALL_OP(READ, WRITE, NAME, DO) \
    assert(frame->hart != NULL); \
    Task* task = (Task*)frame; \
    size_t fd = SYSCALL_ARG(0); \
    VfsFile* file = getFileDescriptor(task->process, fd); \
    if (file != NULL) { \
        if ((file->flags & VFS_FILE_RDONLY) != 0 && WRITE) { \
            SYSCALL_RETURN(-EPERM); \
        } else if ((file->flags & VFS_FILE_WRONLY) != 0 && READ) { \
            SYSCALL_RETURN(-EPERM); \
        } else if (file->functions->NAME != NULL) { \
            DO; \
        } else { \
            SYSCALL_RETURN(-EINVAL); \
        } \
    } else { \
        SYSCALL_RETURN(-EBADF); \
    }


SyscallReturn closeSyscall(TrapFrame* frame) {
    FILE_SYSCALL_OP(false, false, close, {
        file = removeFileDescriptor(task->process, fd);
        file->functions->close(file, task->process);
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
    FILE_SYSCALL_OP(false, false, dup, {
        VfsFile* new_file;
        Error err = file->functions->dup(file, task->process, &new_file);
        if (isError(err)) {
            SYSCALL_RETURN(-err.kind);
        } else {
            int flags = 0;
            if ((SYSCALL_ARG(2) & VFS_OPEN_CLOEXEC) != 0) {
                flags |= VFS_FILE_CLOEXEC;
            }
            if ((SYSCALL_ARG(2) & VFS_OPEN_RDONLY) != 0) {
                flags |= VFS_FILE_RDONLY;
            }
            if ((SYSCALL_ARG(2) & VFS_OPEN_WRONLY) != 0) {
                flags |= VFS_FILE_WRONLY;
            }
            int fd = SYSCALL_ARG(1);
            if (fd < 0) {
                fd = allocateNewFileDescriptor(task->process);
            } else {
                VfsFile* existing = removeFileDescriptor(task->process, fd);
                if (existing != NULL) {
                    existing->functions->close(existing, NULL);
                }
            }
            putNewFileDescriptor(task->process, fd, flags, new_file);
            SYSCALL_RETURN(fd);
        }
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
                            fs->functions->free(fs, NULL);
                        }
                        SYSCALL_RETURN(-err.kind);
                    } else {
                        fs->functions->free(fs, NULL);
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
    SYSCALL_RETURN(SYSCALL_ARG(0));
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
            file_read->base.functions->close((VfsFile*)file_read, NULL);
            SYSCALL_RETURN(-ENOMEM);
        } else {
            int pipe_read = allocateNewFileDescriptor(task->process);
            int pipe_write = allocateNewFileDescriptor(task->process);
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

