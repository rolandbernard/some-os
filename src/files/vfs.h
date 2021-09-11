#ifndef _VFS_H_
#define _VFS_H_

#include "error/error.h"
#include "memory/virtptr.h"
#include "util/spinlock.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    VFS_NODE_DIRECTORY,
    VFS_NODE_REGULAR,
    VFS_NODE_BLOCK,
} VfsNodeKind;

struct VfsNode_s;

typedef void (*VfsFunctionCallbackVoid)(Error error, void* udata);

typedef void (*CloseFunction)(struct VfsNode_s* node, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*DeleteFunction)(struct VfsNode_s* node, VfsFunctionCallbackVoid callback, void* udata);

#define VFS_NODE_BASE_FUNCTIONS \
    CloseFunction close; \
    DeleteFunction delete;

typedef struct {
    VFS_NODE_BASE_FUNCTIONS;
} VfsCommonVtable;

#define VFS_NODE_BASE \
    VfsNodeKind kind; \
    bool virtual;

typedef struct VfsNode_s {
    VFS_NODE_BASE;
    const VfsCommonVtable* functions;
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

typedef void (*VfsFunctionCallbackSizeT)(Error error, size_t size, void* udata);
typedef void (*VfsFunctionCallbackStat)(Error error, VfsStat stat, void* udata);

typedef void (*TellFunction)(struct VfsFile_s* file, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*SeekFunction)(struct VfsFile_s* file, size_t offset, VfsFileSeekFlags flags, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ReadFunction)(struct VfsFile_s* file, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*WriteFunction)(struct VfsFile_s* file, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*StatFileFunction)(struct VfsFile_s* file, VfsFunctionCallbackStat callback, void* udata);

typedef struct {
    VFS_NODE_BASE_FUNCTIONS;
    ReadFunction read;
    WriteFunction write;
    SeekFunction seek;
    TellFunction tell;
    StatFileFunction stat;
} VfsFileVtable;

typedef struct VfsFile_s {
    VFS_NODE_BASE;
    const VfsFileVtable* functions;
} VfsFile;

struct VfsDirectory_s;

typedef struct {
    size_t id;
    char name[];
} VfsDirectoryEntry;

typedef void (*VfsFunctionCallbackEntry)(Error error, VfsDirectoryEntry* entry, void* udata);
typedef void (*VfsFunctionCallbackNode)(Error error, VfsNode* entry, void* udata);

typedef void (*OpenFunction)(struct VfsDirectory_s* dir, const char*, VfsFunctionCallbackNode callback, void* udata);
typedef void (*UnlinkFunction)(struct VfsDirectory_s* dir, const char*, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ReadDirFunction)(struct VfsDirectory_s* dir, VfsFunctionCallbackEntry callback, void* udata);
typedef void (*ResetFunction)(struct VfsDirectory_s* dir, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*CreateDirFunction)(struct VfsDirectory_s* dir, const char*, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*CreateFileFunction)(struct VfsDirectory_s* dir, const char*, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*StatDirFunction)(struct VfsDirectory_s* file, VfsFunctionCallbackStat callback, void* udata);

typedef struct {
    VFS_NODE_BASE_FUNCTIONS;
    CreateDirFunction createdir;
    CreateFileFunction createfile;
    OpenFunction open;
    UnlinkFunction unlink;
    ResetFunction reset;
    ReadDirFunction readdir;
    StatDirFunction stat;
} VfsDirectoryVtable;

typedef struct VfsDirectory_s {
    VFS_NODE_BASE;
    const VfsDirectoryVtable* functions;
} VfsDirectory;

typedef struct {
    char* name;
    VfsNode* node;
} VfsVirtualDirectoryEntry;

typedef struct VfsVirtualDirectory_s {
    VfsDirectory base;
    SpinLock lock;
    VfsStat stats;
    VfsDirectory* mount;
    size_t entry_count;
    VfsVirtualDirectoryEntry* entries;
    size_t read_head;
} VfsVirtualDirectory;

typedef struct {
    VfsFile base;
    SpinLock lock;
    VfsStat stats;
    size_t size;
    uint8_t* data;
} VfsVirtualFile;

Error initVirtualFileSystem();

typedef void (*VfsFunctionCallbackFile)(Error error, VfsFile* entry, void* udata);

typedef void (*VfsFunctionCallbackDirectory)(Error error, VfsDirectory* entry, void* udata);

// These functions will take ownership over the path variable and free it using dealloc
void openFileNamed(char* path, bool create, VfsFunctionCallbackFile callback, void* udata);

void openDirectoryNamed(char* path, bool create, VfsFunctionCallbackDirectory callback, void* udata);

void openNodeNamed(char* path, bool create, VfsFunctionCallbackNode callback, void* udata);

// This is to be used for example for devices
Error insertVirtualNode(char* path, VfsNode* file);

VfsVirtualDirectory* createVirtualDirectory();

VfsVirtualFile* createVirtualFile();

#endif
