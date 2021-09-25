
#include <assert.h>
#include <string.h>

#include "files/syscall.h"
#include "files/vfs.h"
#include "files/path.h"
#include "memory/kalloc.h"
#include "process/schedule.h"

static int allocateNewFileDescriptor(Process* process) {
    int fd = process->resources.next_fd;
    process->resources.next_fd++;
    return fd;
}

static VfsFile* getFileDescriptor(Process* process, int fd) {
    for (size_t i = 0; i < process->resources.fd_count; i++) {
        if (process->resources.fds[i] == fd) {
            return process->resources.files[i];
        }
    }
    return NULL;
}

static void putNewFileDescriptor(Process* process, int fd, VfsFile* file) {
    size_t size = process->resources.fd_count;
    process->resources.files = krealloc(process->resources.files, (size + 1) * sizeof(VfsFile*));
    process->resources.fds = krealloc(process->resources.fds, (size + 1) * sizeof(int));
    process->resources.fd_count++;
    process->resources.fds[size] = fd;
    process->resources.files[size] = file;
}

static void putFileDescriptor(Process* process, int fd, VfsFile* file) {
    for (size_t i = 0; i < process->resources.fd_count; i++) {
        if (process->resources.fds[i] == fd) {
            process->resources.files[i] = file;
            return;
        }
    }
    putNewFileDescriptor(process, fd, file);
}

static void removeFileDescriptor(Process* process, int fd) {
    size_t size = process->resources.fd_count;
    for (size_t i = 0; i < size; i++) {
        if (process->resources.fds[i] == fd) {
            memmove(
                process->resources.fds + i, process->resources.fds + i + 1, (size - i - 1) * sizeof(int)
            );
            memmove(
                process->resources.files + i, process->resources.files + i + 1, (size - i - 1) * sizeof(VfsFile*)
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
        putNewFileDescriptor(process, fd, file);
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
        vfsOpen(
            &global_file_system, process->resources.uid, process->resources.gid, string, args[1],
            args[2], openCallback, process
        );
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
        vfsUnlink(
            &global_file_system, process->resources.uid, process->resources.gid, string, voidSyscallCallback, process
        );
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
            vfsLink(
                &global_file_system, process->resources.uid, process->resources.gid, old, new, voidSyscallCallback, process
            );
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
            vfsRename(
                &global_file_system, process->resources.uid, process->resources.gid, old, new, voidSyscallCallback, process
            );
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

#define FILE_SYSCALL_OP(NAME, DO) \
    assert(frame->hart != NULL); \
    Process* process = (Process*)frame; \
    size_t fd = args[0]; \
    VfsFile* file = getFileDescriptor(process, fd); \
    if (file != NULL) { \
        if (file->functions->NAME != NULL) { \
            moveToSchedState(process, WAITING); \
            DO; \
        } else { \
            process->frame.regs[REG_ARGUMENT_0] = -UNSUPPORTED; \
        } \
    } else { \
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS; \
    }
    

void closeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(close, {
        removeFileDescriptor(process, fd);
        file->functions->close(
            file, process->resources.uid, process->resources.gid, voidSyscallCallback, process
        );
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
    FILE_SYSCALL_OP(read, {
        file->functions->read(
            file, process->resources.uid, process->resources.gid,
            virtPtrFor(args[1], process->memory.table), args[2], sizeTSyscallCallback, process
        );
    });
}

void writeSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(write, {
        file->functions->write(
            file, process->resources.uid, process->resources.gid,
            virtPtrFor(args[1], process->memory.table), args[2], sizeTSyscallCallback, process
        );
    });
}

void seekSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(seek, {
        file->functions->seek(
            file, process->resources.uid, process->resources.gid, args[1], args[2],
            sizeTSyscallCallback, process
        );
    });
}

static void statSyscallCallback(Error error, VfsStat stat, void* udata) {
    Process* process = (Process*)udata;
    process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    if (!isError(error)) {
        VirtPtr ptr = virtPtrFor(process->frame.regs[REG_ARGUMENT_1], process->memory.table);
        memcpyBetweenVirtPtr(ptr, virtPtrForKernel(&stat), sizeof(VfsStat));
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void statSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(stat, {
        file->functions->stat(
            file, process->resources.uid, process->resources.gid, statSyscallCallback, process
        );
    });
}

static void dupCallback(Error error, VfsFile* file, void* udata) {
    Process* process = (Process*)udata;
    if (isError(error)) {
        process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else if ((intptr_t)process->frame.regs[REG_ARGUMENT_2] < 0) {
        size_t fd = allocateNewFileDescriptor(process);
        putNewFileDescriptor(process, fd, file);
        process->frame.regs[REG_ARGUMENT_0] = fd;
    } else {
        size_t fd = process->frame.regs[REG_ARGUMENT_2];
        VfsFile* existing = getFileDescriptor(process, fd);
        if (existing == NULL) {
            putNewFileDescriptor(process, fd, file);
            process->frame.regs[REG_ARGUMENT_0] = fd;
        } else {
            putFileDescriptor(process, fd, file);
            process->frame.regs[REG_ARGUMENT_0] = fd;
            file->functions->close(
                file, process->resources.uid, process->resources.gid, voidSyscallCallback, process
            );
        }
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void dupSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(dup, {
        file->functions->dup(
            file, process->resources.uid, process->resources.gid, dupCallback, process
        );
    });
}

void truncSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(trunc, {
        file->functions->trunc(
            file, process->resources.uid, process->resources.gid, args[1], voidSyscallCallback, process
        );
    });
}

void chmodSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(chmod, {
        file->functions->chmod(
            file, process->resources.uid, process->resources.gid, args[1], voidSyscallCallback, process
        );
    });
}

void chownSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    FILE_SYSCALL_OP(chown, {
        file->functions->chown(
            file, process->resources.uid, process->resources.gid, args[1], args[2], voidSyscallCallback, process
        );
    });
}

static void freeFilesystemCallback(Error error, void* to_free) {
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
                fs->functions->free(fs, 0, 0, freeFilesystemCallback, NULL);
            }
            process->frame.regs[REG_ARGUMENT_0] = -error.kind;
        } else {
            fs->functions->free(fs, 0, 0, freeFilesystemCallback, NULL);
            process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
        }
    }
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void mountSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    char* source = copyPathFromSyscallArgs(process, args[0]);
    if (source != NULL) {
        char* type = copyStringFromSyscallArgs(process, args[2]);
        if (type != NULL) {
            moveToSchedState(process, WAITING);
            createFilesystemFrom(
                &global_file_system, source, type, virtPtrFor(args[3], process->memory.table), mountCreateFsCallback, process
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

char* copyPathFromSyscallArgs(Process* process, uintptr_t ptr) {
    VirtPtr str = virtPtrFor(ptr, process->memory.table);
    bool relative = true;
    size_t length = strlenVirtPtr(str);
    if (readInt(str, 8) == '/') {
        relative = false;
    }
    char* string = kalloc(length + 1 + (relative ? 1 : 0));
    if (string != NULL) {
        if (relative) {
            // TODO: add working directory
            string[0] = '/';
            memcpyBetweenVirtPtr(virtPtrForKernel(string + 1), str, length);
            string[length + 1] = 0;
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

