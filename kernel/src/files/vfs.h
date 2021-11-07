#ifndef _VFS_H_
#define _VFS_H_

#include <stddef.h>
#include <stdint.h>

#include "error/error.h"
#include "memory/virtptr.h"
#include "util/spinlock.h"
#include "interrupt/timer.h"

typedef enum {
    VFS_TYPE_UNKNOWN = 0,
    VFS_TYPE_FIFO = 1,
    VFS_TYPE_CHAR = 2,
    VFS_TYPE_DIR = 4,
    VFS_TYPE_BLOCK = 6,
    VFS_TYPE_REG = 8,
    VFS_TYPE_LNK = 10,
    VFS_TYPE_SOCK = 12,
    VFS_TYPE_MT = 15,
} VfsFileType;

typedef enum {
    VFS_SEEK_CUR = 0,
    VFS_SEEK_SET = 1,
    VFS_SEEK_END = 2,
} VfsSeekWhence;

typedef enum {
    VFS_OPEN_CREATE = (1 << 0),
    VFS_OPEN_APPEND = (1 << 1),
    VFS_OPEN_TRUNC = (1 << 2),
    VFS_OPEN_DIRECTORY = (1 << 3),
    VFS_OPEN_READ = (1 << 4),
    VFS_OPEN_WRITE = (1 << 5),
    VFS_OPEN_EXECUTE = (1 << 6),
    VFS_OPEN_REGULAR = (1 << 7),
    VFS_OPEN_CLOEXEC = (1 << 8),
    VFS_OPEN_EXCL = (1 << 9),
    VFS_OPEN_RDONLY = (1 << 10),
    VFS_OPEN_WRONLY = (1 << 11),
} VfsOpenFlags;

#define OPEN_ACCESS(open_flags) ((open_flags >> 4) & 0b111)

typedef enum {
    VFS_ACCESS_R = (1 << 0),
    VFS_ACCESS_W = (1 << 1),
    VFS_ACCESS_X = (1 << 2),
    VFS_ACCESS_REG = (1 << 3),
    VFS_ACCESS_DIR = (1 << 4),
} VfsAccessFlags;

typedef enum {
    VFS_MODE_A_X = (1 << 0),
    VFS_MODE_A_W = (1 << 1),
    VFS_MODE_A_R = (1 << 2),
    VFS_MODE_G_X = (1 << 3),
    VFS_MODE_G_W = (1 << 4),
    VFS_MODE_G_R = (1 << 5),
    VFS_MODE_O_X = (1 << 6),
    VFS_MODE_O_W = (1 << 7),
    VFS_MODE_O_R = (1 << 8),
    VFS_MODE_STICKY = (1 << 9),
    VFS_MODE_SETUID = (1 << 10),
    VFS_MODE_SETGID = (1 << 11),
    VFS_MODE_TYPE = (0xf << 12),
} VfsModeFlags;

typedef enum {
    VFS_FILE_CLOEXEC = (1 << 0),
    VFS_FILE_RDONLY = (1 << 1),
    VFS_FILE_WRONLY = (1 << 2),
} VfsFileFlags;

#define VFS_MODE_A_RW (VFS_MODE_A_R | VFS_MODE_A_W)
#define VFS_MODE_G_RW (VFS_MODE_G_R | VFS_MODE_G_W)
#define VFS_MODE_O_RW (VFS_MODE_O_R | VFS_MODE_O_W)
#define VFS_MODE_OG_RW (VFS_MODE_O_RW | VFS_MODE_G_RW)
#define VFS_MODE_OGA_RW (VFS_MODE_OG_RW | VFS_MODE_A_RW)

#define MODE_TYPE(mode) (mode >> 12)
#define TYPE_MODE(type) (type << 12)

typedef uint16_t VfsMode;
typedef int Uid;
typedef int Gid;

typedef struct {
    size_t id;
    VfsMode mode;
    size_t nlinks;
    Uid uid;
    Gid gid;
    size_t size;
    size_t block_size;
    Time st_atime;
    Time st_mtime;
    Time st_ctime;
    size_t dev;
} VfsStat;

typedef struct {
    size_t id;
    size_t off;
    size_t len;
    VfsFileType type;
    char name[];
} VfsDirectoryEntry;

struct VfsFile_s;
struct VfsFilesystem_s;
struct Process_s;

typedef void (*VfsFunctionCallbackVoid)(Error error, void* udata);
typedef void (*VfsFunctionCallbackSizeT)(Error error, size_t size, void* udata);
typedef void (*VfsFunctionCallbackStat)(Error error, VfsStat stat, void* udata);
typedef void (*VfsFunctionCallbackFile)(Error error, struct VfsFile_s* entry, void* udata);
typedef void (*VfsFunctionCallbackFilesystem)(Error error, struct VfsFilesystem_s* entry, void* udata);

typedef void (*SeekFunction)(struct VfsFile_s* file, struct Process_s* process, size_t offset, VfsSeekWhence whence, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*ReadFunction)(struct VfsFile_s* file, struct Process_s* process, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*WriteFunction)(struct VfsFile_s* file, struct Process_s* process, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*StatFunction)(struct VfsFile_s* file, struct Process_s* process, VfsFunctionCallbackStat callback, void* udata);
typedef void (*CloseFunction)(struct VfsFile_s* file, struct Process_s* process, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*DupFunction)(struct VfsFile_s* file, struct Process_s* process, VfsFunctionCallbackFile callback, void* udata);
typedef void (*TruncFunction)(struct VfsFile_s* file, struct Process_s* process, size_t size, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ChmodFunction)(struct VfsFile_s* file, struct Process_s* process, VfsMode mode, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ChownFunction)(struct VfsFile_s* file, struct Process_s* process, Uid new_uid, Gid new_gid, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ReaddirFunction)(struct VfsFile_s* file, struct Process_s* process, VirtPtr buff, size_t size, VfsFunctionCallbackSizeT callback, void* udata);

typedef struct {
    SeekFunction seek;
    ReadFunction read;
    WriteFunction write;
    CloseFunction close;
    StatFunction stat;
    DupFunction dup;
    TruncFunction trunc;
    ChmodFunction chmod;
    ChownFunction chown;
    ReaddirFunction readdir;
} VfsFileVtable;

typedef struct VfsFile_s {
    struct VfsFile_s* next;
    const VfsFileVtable* functions;
    int fd;
    int flags;
    size_t ino;
    VfsMode mode;
    Uid uid;
    Gid gid;
    char* path;
} VfsFile;

typedef void (*OpenFunction)(struct VfsFilesystem_s* fs, struct Process_s* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata);
typedef void (*UnlinkFunction)(struct VfsFilesystem_s* fs, struct Process_s* process, const char* path, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*LinkFunction)(struct VfsFilesystem_s* fs, struct Process_s* process, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*RenameFunction)(struct VfsFilesystem_s* fs, struct Process_s* process, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*FreeFunction)(struct VfsFilesystem_s* fs, struct Process_s* process, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*InitFunction)(struct VfsFilesystem_s* fs, struct Process_s* process, VfsFunctionCallbackVoid callback, void* udata);

typedef struct {
    OpenFunction open;
    UnlinkFunction unlink;
    LinkFunction link;
    RenameFunction rename;
    FreeFunction free;
    InitFunction init;
} VfsFilesystemVtable;

typedef struct VfsFilesystem_s {
    const VfsFilesystemVtable* functions;
    size_t open_files;
} VfsFilesystem;

typedef enum {
    MOUNT_TYPE_FS,
    MOUNT_TYPE_FILE,
    MOUNT_TYPE_BIND,
} FilesystemMountType;

typedef struct {
    FilesystemMountType type;
    char* path;
    void* data;
} FilesystemMount;

typedef struct VirtualFilesystem_s {
    struct VirtualFilesystem_s* parent;
    SpinLock lock;
    size_t mount_count;
    FilesystemMount* mounts;
} VirtualFilesystem;

extern VirtualFilesystem global_file_system;

Error initVirtualFileSystem();

VirtualFilesystem* createVirtualFilesystem();

void freeVirtualFilesystem(VirtualFilesystem* fs);

Error mountFilesystem(VirtualFilesystem* fs, VfsFilesystem* filesystem, const char* path);

Error mountFile(VirtualFilesystem* fs, VfsFile* file, const char* path);

Error mountRedirect(VirtualFilesystem* fs, const char* from, const char* to);

Error umount(VirtualFilesystem* fs, const char* from);

void vfsOpen(VirtualFilesystem* fs, struct Process_s* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata);

void vfsUnlink(VirtualFilesystem* fs, struct Process_s* process, const char* path, VfsFunctionCallbackVoid callback, void* udata);

void vfsLink(VirtualFilesystem* fs, struct Process_s* process, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

void vfsRename(VirtualFilesystem* fs, struct Process_s* process, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

// Utility function that calls seek and read on a file to read at a specific offset
void vfsReadAt(VfsFile* file, struct Process_s* process, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata);

// Utility function that calls seek and write on a file to write at a specific offset
void vfsWriteAt(VfsFile* file, struct Process_s* process, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata);

bool canAccess(VfsMode mode, Uid file_uid, Gid file_gid, struct Process_s* process, VfsAccessFlags flags);

void createFilesystemFrom(VirtualFilesystem* fs,const char* path, const char* type, VirtPtr data, VfsFunctionCallbackFilesystem callback, void* udata);

#endif
