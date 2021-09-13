#ifndef _VFS_H_
#define _VFS_H_

#include <stddef.h>
#include <stdint.h>

#include "error/error.h"
#include "memory/virtptr.h"
#include "process/types.h"
#include "util/spinlock.h"
#include "interrupt/timer.h"

typedef enum {
    VFS_TYPE_UNKNOWN = 0,
    VFS_TYPE_REG = 8,
    VFS_TYPE_DIR = 4,
    VFS_TYPE_BLOCK = 2,
    VFS_TYPE_CHAR = 3,
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
    VFS_OPEN_NOATIME = (1 << 4),
} VfsOpenFlags;

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

#define VFS_MODE_A_RW (VFS_MODE_A_R | VFS_MODE_A_W)
#define VFS_MODE_G_RW (VFS_MODE_G_R | VFS_MODE_G_W)
#define VFS_MODE_O_RW (VFS_MODE_O_R | VFS_MODE_O_W)
#define VFS_MODE_OG_RW (VFS_MODE_O_RW | VFS_MODE_G_RW)
#define VFS_MODE_OGA_RW (VFS_MODE_OG_RW | VFS_MODE_A_RW)

#define MODE_TYPE(mode) (mode >> 12)
#define TYPE_MODE(type) (type << 12)

typedef uint16_t VfsMode;

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
} VfsStat;

typedef struct {
    size_t id;
    size_t off;
    size_t len;
    VfsFileType type;
    char name[];
} VfsDirectoryEntry;

struct VfsFile_s;

typedef void (*VfsFunctionCallbackVoid)(Error error, void* udata);
typedef void (*VfsFunctionCallbackSizeT)(Error error, size_t size, void* udata);
typedef void (*VfsFunctionCallbackStat)(Error error, VfsStat stat, void* udata);
typedef void (*VfsFunctionCallbackFile)(Error error, struct VfsFile_s* entry, void* udata);

typedef void (*SeekFunction)(struct VfsFile_s* file, Uid uid, Gid gid, size_t offset, VfsSeekWhence whence, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*ReadFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*WriteFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*StatFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VfsFunctionCallbackStat callback, void* udata);
typedef void (*CloseFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*DupFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VfsFunctionCallbackFile callback, void* udata);

typedef struct {
    SeekFunction seek;
    ReadFunction read;
    WriteFunction write;
    CloseFunction close;
    StatFunction stat;
    DupFunction dup;
} VfsFileVtable;

typedef struct VfsFile_s {
    const VfsFileVtable* functions;
} VfsFile;

struct VfsFilesystem_s;

typedef void (*OpenFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata);
typedef void (*UnlinkFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*LinkFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*RenameFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*FreeFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*InitFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata);

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

void vfsOpen(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata);

void vfsUnlink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata);

void vfsLink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

void vfsRename(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

// Utility function that calls seek and read on a file to read at a specific offset
void vfsReadAt(VfsFile* file, Uid uid, Gid gid, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata);

// Utility function that calls seek and write on a file to write at a specific offset
void vfsWriteAt(VfsFile* file, Uid uid, Gid gid, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata);

#endif
