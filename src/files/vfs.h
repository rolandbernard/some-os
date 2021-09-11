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
    VFS_TYPE_DIR = 1,
    VFS_TYPE_REG = 2,
    VFS_TYPE_BLOCK = 3,
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

typedef void (*SeekFunction)(struct VfsFile_s* file, Uid uid, Gid gid, size_t offset, VfsSeekWhence whence, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*ReadFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*WriteFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*StatFileFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VfsFunctionCallbackStat callback, void* udata);
typedef void (*CloseFunction)(struct VfsFile_s* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata);

typedef struct {
    SeekFunction seek;
    ReadFunction read;
    WriteFunction write;
    CloseFunction close;
    StatFileFunction stat;
} VfsFileVtable;

typedef struct VfsNode_s {
    VfsFileVtable* functions;
} VfsFile;

struct VfsFilesystem_s;

typedef void (*VfsFunctionCallbackFile)(Error error, VfsFile* entry, void* udata);

typedef void (*OpenFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata);
typedef void (*UnlinkFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*LinkFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*RenameFunction)(struct VfsFilesystem_s* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

typedef struct {
    OpenFunction open;
    UnlinkFunction unlink;
    LinkFunction link;
    RenameFunction rename;
} VfsFilesystemVtable;

typedef struct VfsFilesystem_s {
    const VfsFilesystemVtable* functions;
} VfsFilesystem;

typedef enum {
    MOUNT_TYPE_FS,
    MOUNT_TYPE_FILE,
    MOUNT_TYPE_BIND,
} FilesystemMountType;

typedef struct {
    FilesystemMountType type;
    char* from;
    void* to;
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

#endif
