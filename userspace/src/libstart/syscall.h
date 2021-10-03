#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>

typedef enum {
    SYSCALL_PRINT = 0,
    SYSCALL_EXIT = 1,
    SYSCALL_YIELD = 2,
    SYSCALL_FORK = 3,
    SYSCALL_SLEEP = 4,
    SYSCALL_OPEN = 5,
    SYSCALL_LINK = 6,
    SYSCALL_UNLINK = 7,
    SYSCALL_RENAME = 8,
    SYSCALL_CLOSE = 9,
    SYSCALL_READ = 10,
    SYSCALL_WRITE = 11,
    SYSCALL_SEEK = 12,
    SYSCALL_STAT = 13,
    SYSCALL_DUP = 14,
    SYSCALL_TRUNC = 15,
    SYSCALL_CHMOD = 16,
    SYSCALL_CHOWN = 17,
    SYSCALL_MOUNT = 18,
    SYSCALL_UMOUNT = 19,
    SYSCALL_EXECVE = 20,
    SYSCALL_READDIR = 21,
    SYSCALL_GETPID = 22,
    SYSCALL_GETPPID = 23,
    SYSCALL_WAIT = 24,
    SYSCALL_SBRK = 25,
    SYSCALL_PROTECT = 26,
} Syscalls;

uintptr_t make_syscall(
    uintptr_t kind, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5, uintptr_t a6
);

void syscall_print(const char* string);

noreturn void syscall_exit(int status);

void syscall_yield();

int syscall_fork();

uint64_t syscall_sleep(uint64_t nanoseconds);

typedef enum {
    FILE_OPEN_CREATE = (1 << 0),
    FILE_OPEN_APPEND = (1 << 1),
    FILE_OPEN_TRUNC = (1 << 2),
    FILE_OPEN_DIRECTORY = (1 << 3),
    FILE_OPEN_READ = (1 << 4),
    FILE_OPEN_WRITE = (1 << 5),
    FILE_OPEN_EXECUTE = (1 << 6),
    FILE_OPEN_REGULAR = (1 << 7),
    FILE_OPEN_CLOEXEC = (1 << 8),
    FILE_OPEN_EXCL = (1 << 9),
} SyscallOpenFlags;

int syscall_open(const char* path, SyscallOpenFlags flags, uint16_t mode);

int syscall_link(const char* old, const char* new);

int syscall_unlink(const char* path);

int syscall_rename(const char* old, const char* new);

int syscall_close(int fd);

size_t syscall_read(int fd, void* buff, size_t size);

size_t syscall_write(int fd, void* buff, size_t size);

typedef enum {
    FILE_SEEK_CUR = 0,
    FILE_SEEK_SET = 1,
    FILE_SEEK_END = 2,
} SyscallSeekWhence;

size_t syscall_seek(int fd, size_t off, SyscallSeekWhence whence);

typedef struct {
    size_t id;
    uint16_t mode;
    size_t nlinks;
    int uid;
    int gid;
    size_t size;
    size_t block_size;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
} SyscallStat;

int syscall_stat(int fd, SyscallStat* buff);

int syscall_dup(int fd, int newfd, int flags);

int syscall_trunc(int fd, size_t size);

int syscall_chmod(int fd, uint16_t mode);

int syscall_chown(int fd, int uid, int gid);

typedef enum {
    FILE_TYPE_UNKNOWN = 0,
    FILE_TYPE_REG = 8,
    FILE_TYPE_DIR = 4,
    FILE_TYPE_BLOCK = 2,
    FILE_TYPE_CHAR = 3,
} FileType;

typedef struct {
    size_t id;
    size_t off;
    size_t len;
    FileType type;
    char name[];
} DirectoryEntry;

size_t syscall_readdir(int fd, DirectoryEntry* dirent, size_t size);

int syscall_mount(const char* source, const char* target, const char* type, void* data);

int syscall_umount(const char* path);

int syscall_execve(const char* path, char const* args[], char const* envs[]);

int syscall_getpid();

int syscall_getppid();

int syscall_wait(int pid, int* status);

void* syscall_sbrk(intptr_t change);

typedef enum {
    PROT_NONE = 0,
    PROT_READ = 4,
    PROT_WRITE = 2,
    PROT_EXEC = 1,
} SyscallProtect;

int syscall_protect(void* addr, size_t len, SyscallProtect prot);

#endif
