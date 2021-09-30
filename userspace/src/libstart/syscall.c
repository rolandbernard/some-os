
#include "libstart/syscall.h"

void syscall_print(const char* string) {
    SYSCALL(SYSCALL_PRINT, (uintptr_t)string);
}

void syscall_exit(int status) {
    for (;;) {
        SYSCALL(SYSCALL_EXIT, status);
    }
}

void syscall_yield() {
    SYSCALL(SYSCALL_YIELD);
}

int syscall_fork() {
    return SYSCALL(SYSCALL_FORK);
}

uint64_t syscall_sleep(uint64_t nanoseconds) {
    return SYSCALL(SYSCALL_SLEEP, nanoseconds);
}

int syscall_open(const char* path, SyscallOpenFlags flags, uint16_t mode) {
    return SYSCALL(SYSCALL_OPEN, (uintptr_t)path, flags, mode);
}

int syscall_link(const char* old, const char* new) {
    return SYSCALL(SYSCALL_LINK, (uintptr_t)old, (uintptr_t)new);
}

int syscall_unlink(const char* path) {
    return SYSCALL(SYSCALL_UNLINK, (uintptr_t)path);
}

int syscall_rename(const char* old, const char* new) {
    return SYSCALL(SYSCALL_RENAME, (uintptr_t)old, (uintptr_t)new);
}

int syscall_close(int fd) {
    return SYSCALL(SYSCALL_CLOSE, fd);
}

size_t syscall_read(int fd, void* buff, size_t size) {
    return SYSCALL(SYSCALL_READ, fd, (uintptr_t)buff, size);
}

size_t syscall_write(int fd, void* buff, size_t size) {
    return SYSCALL(SYSCALL_WRITE, fd, (uintptr_t)buff, size);
}

size_t syscall_seek(int fd, size_t off, SyscallSeekWhence whence) {
    return SYSCALL(SYSCALL_SEEK, fd, off, whence);
}

int syscall_stat(int fd, SyscallStat* buff) {
    return SYSCALL(SYSCALL_STAT, fd, (uintptr_t)buff);
}

int syscall_dup(int fd, int newfd) {
    return SYSCALL(SYSCALL_DUP, fd, newfd);
}

int syscall_trunc(int fd, size_t size) {
    return SYSCALL(SYSCALL_TRUNC, fd, size);
}

int syscall_chmod(int fd, uint16_t mode) {
    return SYSCALL(SYSCALL_CHMOD, fd, mode);
}

int syscall_chown(int fd, int uid, int gid) {
    return SYSCALL(SYSCALL_CHOWN, fd, uid, gid);
}

size_t syscall_readdir(int fd, DirectoryEntry* dirent, size_t size) {
    return SYSCALL(SYSCALL_READDIR, fd, (uintptr_t)dirent, size);
}

int syscall_mount(const char* source, const char* target, const char* type, void* data) {
    return SYSCALL(SYSCALL_MOUNT, (uintptr_t)source, (uintptr_t)target, (uintptr_t)type, (uintptr_t)data);
}

int syscall_umount(const char* path) {
    return SYSCALL(SYSCALL_UMOUNT, (uintptr_t)path);
}

int syscall_execve(const char* path, char const* args[], char const* envs[]) {
    return SYSCALL(SYSCALL_EXECVE, (uintptr_t)path, (uintptr_t)args, (uintptr_t)envs);
}

int syscall_getpid() {
    return SYSCALL(SYSCALL_GETPID);
}

int syscall_getppid() {
    return SYSCALL(SYSCALL_GETPPID);
}

