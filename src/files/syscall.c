
#include <assert.h>
#include <string.h>

#include "files/syscall.h"

#include "files/vfs.h"
#include "files/path.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "process/schedule.h"
#include "util/util.h"
#include "files/special/pipe.h"

static int allocateNewFileDescriptor(Process* process) {
    int fd = process->resources.next_fd;
    process->resources.next_fd++;
    return fd;
}

static VfsFile* getFileDescriptor(Process* process, int fd) {
    for (size_t i = 0; i < process->resources.fd_count; i++) {
        if (process->resources.filedes[i]->fd == fd) {
            return process->resources.filedes[i];
        }
    }
    return NULL;
}

static void putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file) {
    size_t size = process->resources.fd_count;
    process->resources.filedes = krealloc(process->resources.filedes, (size + 1) * sizeof(VfsFile*));
    process->resources.fd_count++;
    process->resources.filedes[size] = file;
    file->fd = fd;
    file->flags = flags;
}

static void putFileDescriptor(Process* process, int fd, int flags, VfsFile* file) {
    for (size_t i = 0; i < process->resources.fd_count; i++) {
        if (process->resources.filedes[i]->fd == fd) {
            process->resources.filedes[i] = file;
            file->fd = fd;
            file->flags = flags;
            return;
        }
    }
    putNewFileDescriptor(process, fd, flags, file);
}

static void removeFileDescriptor(Process* process, int fd) {
    size_t size = process->resources.fd_count;
    for (size_t i = 0; i < size; i++) {
        if (process->resources.filedes[i]->fd == fd) {
            memmove(
                process->resources.filedes + i, process->resources.filedes + i + 1, (size - i - 1) * sizeof(VfsFile*)
            );
            process->resources.fd_count--;
            return;
        }
    }
}

static void openCallback(Error error, VfsFile* file, void* udata) {
    Process* process = (Process*)udata;
    if (isError(error)) {
        process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        size_t fd = allocateNewFileDescriptor(process);
        int flags = 0;
        if ((process->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_CLOEXEC) != 0) {
            flags |= VFS_FILE_CLOEXEC;
        }
        if ((process->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_RDONLY) != 0) {
            flags |= VFS_FILE_RDONLY;
        }
        if ((process->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_WRONLY) != 0) {
            flags |= VFS_FILE_WRONLY;
        }
        putNewFileDescriptor(process, fd, flags, file);
        process->frame.regs[REG_ARGUMENT_0] = fd;
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void openSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    char* string = copyPathFromSyscallArgs(process, args[0]);
    if (string != NULL) {
        moveToSchedState(process, WAITING);
        vfsOpen(&global_file_system, process, string, args[1], args[2], openCallback, process);
        dealloc(string);
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
    }
}

static void voidSyscallCallback(Error error, void* udata) {
    Process* process = (Process*)udata;
    process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void unlinkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    char* string = copyPathFromSyscallArgs(process, args[0]);
    if (string != NULL) {
        moveToSchedState(process, WAITING);
        vfsUnlink(&global_file_system, process, string, voidSyscallCallback, process);
        dealloc(string);
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
    }
}

void linkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    char* old = copyPathFromSyscallArgs(process, args[0]);
    if (old != NULL) {
        char* new = copyPathFromSyscallArgs(process, args[1]);
        if (new != NULL) {
            moveToSchedState(process, WAITING);
            vfsLink(&global_file_system, process, old, new, voidSyscallCallback, process);
            dealloc(new);
            dealloc(old);
        } else {
            dealloc(old);
            process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
        }
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
    }
}

void renameSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    char* old = copyPathFromSyscallArgs(process, args[0]);
    if (old != NULL) {
        char* new = copyPathFromSyscallArgs(process, args[1]);
        if (new != NULL) {
            moveToSchedState(process, WAITING);
            vfsRename(&global_file_system, process, old, new, voidSyscallCallback, process);
            dealloc(new);
            dealloc(old);
        } else {
            dealloc(old);
            process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
        }
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
    }
}

#define FILE_SYSCALL_OP(READ, WRITE, NAME, DO) \
    assert(frame->hart != NULL); \
    Process* process = (Process*)frame; \
    size_t fd = args[0]; \
    VfsFile* file = getFileDescriptor(process, fd); \
    if (file != NULL) { \
        if ((file->flags & VFS_FILE_RDONLY) != 0 && WRITE) { \
            process->frame.regs[REG_ARGUMENT_0] = -UNSUPPORTED; \
        } else if ((file->flags & VFS_FILE_WRONLY) != 0 && READ) { \
            process->frame.regs[REG_ARGUMENT_0] = -UNSUPPORTED; \
        } else if (file->functions->NAME != NULL) { \
            moveToSchedState(process, WAITING); \
            DO; \
        } else { \
            process->frame.regs[REG_ARGUMENT_0] = -UNSUPPORTED; \
        } \
    } else { \
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS; \
    }
    

void closeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, close, {
        removeFileDescriptor(process, fd);
        file->functions->close(file, process, voidSyscallCallback, process);
    });
}

static void sizeTSyscallCallback(Error error, size_t size, void* udata) {
    Process* process = (Process*)udata;
    if (isError(error)) {
        process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        process->frame.regs[REG_ARGUMENT_0] = size;
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void readSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(true, false, read, {
        file->functions->read(
            file, process, virtPtrFor(args[1], process->memory.mem),
            args[2], sizeTSyscallCallback, process
        );
    });
}

void writeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, write, {
        file->functions->write(
            file, process, virtPtrFor(args[1], process->memory.mem),
            args[2], sizeTSyscallCallback, process
        );
    });
}

void seekSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, seek, {
        file->functions->seek(file, process, args[1], args[2], sizeTSyscallCallback, process);
    });
}

static void statSyscallCallback(Error error, VfsStat stat, void* udata) {
    Process* process = (Process*)udata;
    process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    if (!isError(error)) {
        VirtPtr ptr = virtPtrFor(process->frame.regs[REG_ARGUMENT_2], process->memory.mem);
        memcpyBetweenVirtPtr(ptr, virtPtrForKernel(&stat), sizeof(VfsStat));
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void statSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, stat, {
        file->functions->stat(file, process, statSyscallCallback, process);
    });
}

static void dupCallback(Error error, VfsFile* file, void* udata) {
    Process* process = (Process*)udata;
    if (isError(error)) {
        process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        int flags = 0;
        if ((process->frame.regs[REG_ARGUMENT_3] & VFS_OPEN_CLOEXEC) != 0) {
            flags |= VFS_FILE_CLOEXEC;
        }
        if ((process->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_RDONLY) != 0) {
            flags |= VFS_FILE_RDONLY;
        }
        if ((process->frame.regs[REG_ARGUMENT_2] & VFS_OPEN_WRONLY) != 0) {
            flags |= VFS_FILE_WRONLY;
        }
        if ((int)process->frame.regs[REG_ARGUMENT_2] < 0) {
            size_t fd = allocateNewFileDescriptor(process);
            putNewFileDescriptor(process, fd, flags, file);
            process->frame.regs[REG_ARGUMENT_0] = fd;
        } else {
            size_t fd = process->frame.regs[REG_ARGUMENT_2];
            VfsFile* existing = getFileDescriptor(process, fd);
            if (existing == NULL) {
                putNewFileDescriptor(process, fd, flags, file);
                process->frame.regs[REG_ARGUMENT_0] = fd;
            } else {
                putFileDescriptor(process, fd, flags, file);
                process->frame.regs[REG_ARGUMENT_0] = fd;
                file->functions->close(file, process, voidSyscallCallback, process);
            }
        }
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void dupSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, false, dup, {
        file->functions->dup(file, process, dupCallback, process);
    });
}

void truncSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, trunc, {
        file->functions->trunc(file, process, args[1], voidSyscallCallback, process);
    });
}

void chmodSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, chmod, {
        file->functions->chmod(file, process, args[1], voidSyscallCallback, process);
    });
}

void chownSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(false, true, chown, {
        file->functions->chown(file, process, args[1], args[2], voidSyscallCallback, process);
    });
}

void readdirSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(true, false, readdir, {
        file->functions->readdir(
            file, process, virtPtrFor(args[1], process->memory.mem),
            args[2], sizeTSyscallCallback, process
        );
    });
}

static void mountCreateFsCallback(Error error, VfsFilesystem* fs, void* udata) {
    Process* process = (Process*)udata;
    if (isError(error)) {
        process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        char* target = copyPathFromSyscallArgs(process, process->frame.regs[REG_ARGUMENT_2]);
        if (target != NULL) {
            error = mountFilesystem(&global_file_system, fs, target);
            dealloc(target);
            if (isError(error)) {
                fs->functions->free(fs, NULL, noop, NULL);
            }
            process->frame.regs[REG_ARGUMENT_0] = -error.kind;
        } else {
            fs->functions->free(fs, NULL, noop, NULL);
            process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
        }
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void mountSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    if (process->resources.uid != 0 && process->resources.gid != 0) { // Only root can mount
        process->frame.regs[REG_ARGUMENT_0] = -FORBIDDEN;
    } else {
        char* source = copyPathFromSyscallArgs(process, args[0]);
        if (source != NULL) {
            char* type = copyStringFromSyscallArgs(process, args[2]);
            if (type != NULL) {
                moveToSchedState(process, WAITING);
                createFilesystemFrom(
                    &global_file_system, source, type, virtPtrFor(args[3], process->memory.mem), mountCreateFsCallback, process
                );
                dealloc(type);
                dealloc(source);
            } else {
                dealloc(source);
                process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
            }
        } else {
            process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
        }
    }
}

void umountSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    if (process->resources.uid != 0 && process->resources.gid != 0) { // Only root can mount
        process->frame.regs[REG_ARGUMENT_0] = -FORBIDDEN;
    } else {
        char* path = copyPathFromSyscallArgs(process, args[0]);
        if (path != NULL) {
            process->frame.regs[REG_ARGUMENT_0] = -umount(&global_file_system, path).kind;
            dealloc(path);
        } else {
            process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
        }
    }
}

void chdirSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    char* path = copyPathFromSyscallArgs(process, args[0]);
    if (path != NULL) {
        dealloc(process->resources.cwd);
        process->resources.cwd = path;
        process->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
    }
}

void getcwdSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    VirtPtr buff = virtPtrFor(args[0], process->memory.mem);
    size_t length = args[1];
    size_t cwd_length = strlen(process->resources.cwd);
    memcpyBetweenVirtPtr(
        buff, virtPtrForKernel(process->resources.cwd), umin(length, cwd_length + 1)
    );
    process->frame.regs[REG_ARGUMENT_0] = args[0];
}

char* copyPathFromSyscallArgs(Process* process, uintptr_t ptr) {
    VirtPtr str = virtPtrFor(ptr, process->memory.mem);
    bool relative = true;
    size_t cwd_length = 0;
    if (process->resources.cwd != NULL) {
        cwd_length = strlen(process->resources.cwd);
    }
    size_t length = strlenVirtPtr(str);
    if (readInt(str, 8) == '/') {
        relative = false;
    }
    char* string = kalloc(length + 1 + (relative ? cwd_length + 1 : 0));
    if (string != NULL) {
        if (relative) {
            if (process->resources.cwd != NULL) {
                memcpy(string, process->resources.cwd, cwd_length);
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
    Process* process = (Process*)frame;
    VfsFile* pipe_file = (VfsFile*)createPipeFile();
    if (pipe_file != NULL) {
        int pipe_read = allocateNewFileDescriptor(process);
        int pipe_write = allocateNewFileDescriptor(process);
        putNewFileDescriptor(process, pipe_read, VFS_FILE_RDONLY, pipe_file);
        putNewFileDescriptor(process, pipe_write, VFS_FILE_WRONLY, pipe_file);
        VirtPtr arr = virtPtrFor(args[1], process->memory.mem);
        writeIntAt(arr, sizeof(int) * 8, 0, pipe_read);
        writeIntAt(arr, sizeof(int) * 8, 1, pipe_write);
        process->frame.regs[REG_ARGUMENT_0] = -SUCCESS;
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -ALREADY_IN_USE;
    }
}

