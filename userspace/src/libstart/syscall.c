
#include "libstart/syscall.h"

#define CONSOME(...)
#define KEEP(...) __VA_ARGS__

#define IFN(...) CONSOME __VA_OPT__(()KEEP)

#define IFE(...) KEEP __VA_OPT__(()CONSOME)

#define Z6(...) IFE(__VA_ARGS__)(0, 0, 0, 0, 0, 0) IFN(__VA_ARGS__)(Z5(__VA_ARGS__))
#define Z5(A1, ...) A1, IFE(__VA_ARGS__)(0, 0, 0, 0, 0) IFN(__VA_ARGS__)(Z4(__VA_ARGS__))
#define Z4(A1, ...) A1, IFE(__VA_ARGS__)(0, 0, 0, 0) IFN(__VA_ARGS__)(Z3(__VA_ARGS__))
#define Z3(A1, ...) A1, IFE(__VA_ARGS__)(0, 0, 0) IFN(__VA_ARGS__)(Z2(__VA_ARGS__))
#define Z2(A1, ...) A1, IFE(__VA_ARGS__)(0, 0) IFN(__VA_ARGS__)(Z1(__VA_ARGS__))
#define Z1(A1, ...) A1, IFE(__VA_ARGS__)(0) IFN(__VA_ARGS__)(__VA_ARGS__)

#define SYSCALL(KIND, ...) make_syscall(KIND, Z6(__VA_ARGS__))

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

int syscall_dup(int fd, int newfd, int flags) {
    return SYSCALL(SYSCALL_DUP, fd, newfd, flags);
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

int syscall_wait(int pid, int* status) {
    return SYSCALL(SYSCALL_WAIT, pid, (uintptr_t)status);
}

void* syscall_sbrk(intptr_t change) {
    return (void*)SYSCALL(SYSCALL_SBRK, change);
}

int syscall_protect(void* addr, size_t len, SyscallProtect prot) {
    return SYSCALL(SYSCALL_PROTECT, (uintptr_t)addr, len, prot);
}

int syscall_sigaction(int signal, const SigAction* new, SigAction* old) {
    return SYSCALL(SYSCALL_SIGACTION, signal, (uintptr_t)new, (uintptr_t)old);
}

int syscall_sigreturn() {
    return SYSCALL(SYSCALL_SIGRETURN);
}

int syscall_kill(int pid, int signal) {
    return SYSCALL(SYSCALL_KILL, pid, signal);
}

