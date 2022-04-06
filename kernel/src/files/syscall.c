
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

static void openCallback(Error error, VfsFile* file, void* udata) {
    Task* task = (Task*)udata;
    if (isError(error)) {
        task->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        size_t fd = allocateNewFileDescriptor(task->process);
        int flags = 0;
        if ((task->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_CLOEXEC) != 0) {
            flags |= VFS_FILE_CLOEXEC;
        }
        if ((task->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_RDONLY) != 0) {
            flags |= VFS_FILE_RDONLY;
        }
        if ((task->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_WRONLY) != 0) {
            flags |= VFS_FILE_WRONLY;
        }
        putNewFileDescriptor(task->process, fd, flags, file);
        task->frame.regs[REG_ARGUMENT_0] = fd;
    }
    task->sched.state = ENQUABLE;
    enqueueTask(task);
}

void openSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyPathFromSyscallArgs(task, args[0]);
    if (string != NULL) {
        task->sched.state = WAITING;
        vfsOpen(&global_file_system, task->process, string, args[1], args[2], openCallback, task);
        dealloc(string);
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

static void voidSyscallCallback(Error error, void* udata) {
    Task* task = (Task*)udata;
    task->frame.regs[REG_ARGUMENT_0] = -error.kind;
    task->sched.state = ENQUABLE;
    enqueueTask(task);
}

void unlinkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyPathFromSyscallArgs(task, args[0]);
    if (string != NULL) {
        task->sched.state = WAITING;
        vfsUnlink(&global_file_system, task->process, string, voidSyscallCallback, task);
        dealloc(string);
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

void linkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* old = copyPathFromSyscallArgs(task, args[0]);
    if (old != NULL) {
        char* new = copyPathFromSyscallArgs(task, args[1]);
        if (new != NULL) {
            task->sched.state = WAITING;
            vfsLink(&global_file_system, task->process, old, new, voidSyscallCallback, task);
            dealloc(new);
            dealloc(old);
        } else {
            dealloc(old);
            task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
        }
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

void renameSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* old = copyPathFromSyscallArgs(task, args[0]);
    if (old != NULL) {
        char* new = copyPathFromSyscallArgs(task, args[1]);
        if (new != NULL) {
            task->sched.state = WAITING;
            vfsRename(&global_file_system, task->process, old, new, voidSyscallCallback, task);
            dealloc(new);
            dealloc(old);
        } else {
            dealloc(old);
            task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
        }
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

#define FILE_SYSCALL_OP(READ, WRITE, NAME, DO) \
    assert(frame->hart != NULL); \
    Task* task = (Task*)frame; \
    size_t fd = args[0]; \
    VfsFile* file = getFileDescriptor(task->process, fd); \
    if (file != NULL) { \
        if ((file->flags & VFS_FILE_RDONLY) != 0 && WRITE) { \
            task->frame.regs[REG_ARGUMENT_0] = -EPERM; \
        } else if ((file->flags & VFS_FILE_WRONLY) != 0 && READ) { \
            task->frame.regs[REG_ARGUMENT_0] = -EPERM; \
        } else if (file->functions->NAME != NULL) { \
            task->sched.state = WAITING; \
            DO; \
        } else { \
            task->frame.regs[REG_ARGUMENT_0] = -EINVAL; \
        } \
    } else { \
        task->frame.regs[REG_ARGUMENT_0] = -EBADF; \
    }
    

void closeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, close, {
        file = removeFileDescriptor(task->process, fd);
        file->functions->close(file, task->process, voidSyscallCallback, task);
    });
}

static void sizeTSyscallCallback(Error error, size_t size, void* udata) {
    Task* task = (Task*)udata;
    if (isError(error)) {
        task->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        task->frame.regs[REG_ARGUMENT_0] = size;
    }
    task->sched.state = ENQUABLE;
    enqueueTask(task);
}

void readSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(true, false, read, {
        file->functions->read(
            file, task->process, virtPtrForTask(args[1], task), args[2], sizeTSyscallCallback, task
        );
    });
}

void writeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, write, {
        file->functions->write(
            file, task->process, virtPtrForTask(args[1], task), args[2], sizeTSyscallCallback, task
        );
    });
}

void seekSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, seek, {
        file->functions->seek(file, task->process, args[1], args[2], sizeTSyscallCallback, task);
    });
}

static void statSyscallCallback(Error error, VfsStat stat, void* udata) {
    Task* task = (Task*)udata;
    task->frame.regs[REG_ARGUMENT_0] = -error.kind;
    if (!isError(error)) {
        VirtPtr ptr = virtPtrForTask(task->frame.regs[REG_ARGUMENT_2], task);
        memcpyBetweenVirtPtr(ptr, virtPtrForKernel(&stat), sizeof(VfsStat));
    }
    task->sched.state = ENQUABLE;
    enqueueTask(task);
}

void statSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, stat, {
        file->functions->stat(file, task->process, statSyscallCallback, task);
    });
}

static void dupCallback(Error error, VfsFile* file, void* udata) {
    Task* task = (Task*)udata;
    if (isError(error)) {
        task->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        int flags = 0;
        if ((task->frame.regs[REG_ARGUMENT_3] & VFS_OPEN_CLOEXEC) != 0) {
            flags |= VFS_FILE_CLOEXEC;
        }
        if ((task->frame.regs[REG_ARGUMENT_3] & VFS_OPEN_RDONLY) != 0) {
            flags |= VFS_FILE_RDONLY;
        }
        if ((task->frame.regs[REG_ARGUMENT_3] & VFS_OPEN_WRONLY) != 0) {
            flags |= VFS_FILE_WRONLY;
        }
        if ((int)task->frame.regs[REG_ARGUMENT_2] < 0) {
            size_t fd = allocateNewFileDescriptor(task->process);
            putNewFileDescriptor(task->process, fd, flags, file);
            task->frame.regs[REG_ARGUMENT_0] = fd;
        } else {
            size_t fd = task->frame.regs[REG_ARGUMENT_2];
            VfsFile* existing = removeFileDescriptor(task->process, fd);
            if (existing != NULL) {
                existing->functions->close(existing, NULL, noop, NULL);
            }
            putNewFileDescriptor(task->process, fd, flags, file);
            task->frame.regs[REG_ARGUMENT_0] = fd;
        }
    }
    task->sched.state = ENQUABLE;
    enqueueTask(task);
}

void dupSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, dup, {
        file->functions->dup(file, task->process, dupCallback, task);
    });
}

void truncSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, trunc, {
        file->functions->trunc(file, task->process, args[1], voidSyscallCallback, task);
    });
}

void chmodSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, chmod, {
        file->functions->chmod(file, task->process, args[1], voidSyscallCallback, task);
    });
}

void chownSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, chown, {
        file->functions->chown(file, task->process, args[1], args[2], voidSyscallCallback, task);
    });
}

void readdirSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(true, false, readdir, {
        file->functions->readdir(
            file, task->process, virtPtrForTask(args[1], task), args[2], sizeTSyscallCallback, task
        );
    });
}

static void mountCreateFsCallback(Error error, VfsFilesystem* fs, void* udata) {
    Task* task = (Task*)udata;
    if (isError(error)) {
        task->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        char* target = copyPathFromSyscallArgs(task, task->frame.regs[REG_ARGUMENT_2]);
        if (target != NULL) {
            error = mountFilesystem(&global_file_system, fs, target);
            dealloc(target);
            if (isError(error)) {
                fs->functions->free(fs, NULL, noop, NULL);
            }
            task->frame.regs[REG_ARGUMENT_0] = -error.kind;
        } else {
            fs->functions->free(fs, NULL, noop, NULL);
            task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
        }
    }
    task->sched.state = ENQUABLE;
    enqueueTask(task);
}

void mountSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (
        task->process != NULL && task->process->resources.uid != 0
        && task->process->resources.gid != 0
    ) { // Only root can mount
        task->frame.regs[REG_ARGUMENT_0] = -EACCES;
    } else {
        char* source = copyPathFromSyscallArgs(task, args[0]);
        if (source != NULL) {
            char* type = copyStringFromSyscallArgs(task, args[2]);
            if (type != NULL) {
                task->sched.state = WAITING;
                createFilesystemFrom(
                    &global_file_system, source, type, virtPtrForTask(args[3], task),
                    mountCreateFsCallback, task
                );
                dealloc(type);
                dealloc(source);
            } else {
                dealloc(source);
                task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
            }
        } else {
            task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
        }
    }
}

void umountSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (task->process->resources.uid != 0 && task->process->resources.gid != 0) { // Only root can mount
        task->frame.regs[REG_ARGUMENT_0] = -EACCES;
    } else {
        char* path = copyPathFromSyscallArgs(task, args[0]);
        if (path != NULL) {
            task->frame.regs[REG_ARGUMENT_0] = -umount(&global_file_system, path).kind;
            dealloc(path);
        } else {
            task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
        }
    }
}

void chdirSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* path = copyPathFromSyscallArgs(task, args[0]);
    if (path != NULL) {
        dealloc(task->process->resources.cwd);
        task->process->resources.cwd = path;
        task->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

void getcwdSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    VirtPtr buff = virtPtrForTask(args[0], task);
    size_t length = args[1];
    size_t cwd_length = strlen(task->process->resources.cwd);
    memcpyBetweenVirtPtr(
        buff, virtPtrForKernel(task->process->resources.cwd), umin(length, cwd_length + 1)
    );
    task->frame.regs[REG_ARGUMENT_0] = args[0];
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

void pipeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    PipeFile* file_read = createPipeFile();
    if (file_read != NULL) {
        PipeFile* file_write = duplicatePipeFile(file_read);
        if (file_write == NULL) {
            file_read->base.functions->close((VfsFile*)file_read, NULL, noop, NULL);
            task->frame.regs[REG_ARGUMENT_0] = -ENOMEM;
        } else {
            int pipe_read = allocateNewFileDescriptor(task->process);
            int pipe_write = allocateNewFileDescriptor(task->process);
            putNewFileDescriptor(task->process, pipe_read, VFS_FILE_RDONLY, (VfsFile*)file_read);
            putNewFileDescriptor(task->process, pipe_write, VFS_FILE_WRONLY, (VfsFile*)file_write);
            VirtPtr arr = virtPtrForTask(args[0], task);
            writeIntAt(arr, sizeof(int) * 8, 0, pipe_read);
            writeIntAt(arr, sizeof(int) * 8, 1, pipe_write);
            task->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
        }
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -ENOMEM;
    }
}

void mknodSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyPathFromSyscallArgs(task, args[0]);
    if (string != NULL) {
        task->sched.state = WAITING;
        vfsMknod(&global_file_system, task->process, string, args[1], args[2], voidSyscallCallback, task);
        dealloc(string);
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

