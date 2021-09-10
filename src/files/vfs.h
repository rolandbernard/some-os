#ifndef _VFS_H_
#define _VFS_H_

#include "error/error.h"
#include "memory/virtptr.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    VFS_NODE_DIRECTORY,
    VFS_NODE_REGULAR,
    VFS_NODE_BLOCK,
} VfsNodeKind;

typedef struct {
    VfsNodeKind kind;
} VfsNode;

struct VfsFile_s;

typedef enum {
    VFS_SEEK_SET,
    VFS_SEEK_CUR,
    VFS_SEEK_END,
} VfsFileSeekFlags;

typedef struct {
    size_t id;
    uint16_t mode;
    size_t nlinks;
    uint64_t uid;
    uint64_t gid;
    size_t size;
    size_t block_size;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
} VfsStat;

typedef void (*VfsFunctionCallbackVoid)(Error error, void* udata);
typedef void (*VfsFunctionCallbackSizeT)(Error error, size_t size, void* udata);
typedef void (*VfsFunctionCallbackStat)(Error error, VfsStat stat, void* udata);

typedef void (*TellFunction)(struct VfsFile_s* file, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*SeekFunction)(struct VfsFile_s* file, size_t offset, VfsFileSeekFlags flags, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ReadFunction)(struct VfsFile_s* file, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*WriteFunction)(struct VfsFile_s* file, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*CloseFunction)(struct VfsFile_s* file, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*DeleteFunction)(struct VfsFile_s* file, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*StatFileFunction)(struct VfsFile_s* file, VfsFunctionCallbackStat callback, void* udata);

typedef struct {
    ReadFunction read;
    WriteFunction write;
    SeekFunction seek;
    TellFunction tell;
    CloseFunction close;
    DeleteFunction delete;
    StatFileFunction stat;
} VfsFileVtable;

typedef struct VfsFile_s {
    VfsNode base;
    const VfsFileVtable* functions;
} VfsFile;

struct VfsDirectory_s;

typedef struct {
    size_t id;
    char name[];
} VfsDirectoryEntry;

typedef void (*VfsFunctionCallbackEntry)(Error error, VfsDirectoryEntry* entry, void* udata);
typedef void (*VfsFunctionCallbackNode)(Error error, VfsNode* entry, void* udata);

typedef void (*OpenFunction)(struct VfsDirectory_s* dir, size_t id, VfsFunctionCallbackNode callback, void* udata);
typedef void (*UnlinkFunction)(struct VfsDirectory_s* dir, size_t id, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ReadDirFunction)(struct VfsDirectory_s* dir, VfsFunctionCallbackEntry callback, void* udata);
typedef void (*ResetFunction)(struct VfsDirectory_s* dir, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*CreateFunction)(struct VfsDirectory_s* dir, char* name, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*StatDirFunction)(struct VfsDirectory_s* file, VfsFunctionCallbackStat callback, void* udata);
typedef void (*CloseDirFunction)(struct VfsDirectory_s* file, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*DeleteDirFunction)(struct VfsDirectory_s* file, VfsFunctionCallbackVoid callback, void* udata);

typedef struct {
    CreateFunction create;
    OpenFunction open;
    UnlinkFunction unlink;
    ResetFunction reset;
    ReadDirFunction readdir;
    CloseDirFunction close;
    DeleteDirFunction delete;
    StatDirFunction stat;
} VfsDirectoryVtable;

typedef struct VfsDirectory_s {
    VfsNode base;
    const VfsDirectoryVtable* functions;
} VfsDirectory;

struct VfsVirtualDirectory_s;

typedef struct {
    char* name;
    struct VfsVirtualDirectory_s* node;
} VfsVirtualDirectoryEntry;

typedef struct VfsVirtualDirectory_s {
    VfsDirectory base;
    VfsStat stats;
    size_t parallel_count;
    VfsNode* parallel;
    size_t entry_count;
    VfsVirtualDirectoryEntry* entries;
} VfsVirtualDirectory;

typedef struct {
    VfsFile base;
    VfsStat stats;
    size_t size;
    uint8_t* data;
} VfsVirtualFile;

Error initVirtualFileSystem();

// This function will take ownership over the name variable
void openNodeNamed(char* path, bool create, VfsFunctionCallbackNode callback, void* udata);

// This is to be used for example for devices
Error insertVirtualNode(char* path, VfsNode* file);

VfsVirtualDirectory* createVirtualDirectory();

VfsVirtualFile* createVirtualFile();

#endif
