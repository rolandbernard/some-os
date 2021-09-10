#ifndef _VFS_H_
#define _VFS_H_

#include "error/error.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool directory : 1;
    bool read : 1;
    bool write : 1;
    bool execute : 1;
    bool tty : 1;
} VfsNodeFlags;

#define VFS_BASE_NODE \
    VfsNodeFlags flags;

typedef struct {
    VFS_BASE_NODE;
} VfsNode;

struct VfsFile_s;

typedef enum {
    VFS_SEEK_SET,
    VFS_SEEK_CUR,
    VFS_SEEK_END,
} VfsFileSeekFlags;

typedef void (*VfsFunctionCallbackVoid)(Error error, void* udata);
typedef void (*VfsFunctionCallbackSizeT)(Error error, size_t size, void* udata);

typedef void (*TellFunction)(struct VfsFile_s* file, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*SeekFunction)(struct VfsFile_s* file, size_t offset, VfsFileSeekFlags flags, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*ReadFunction)(struct VfsFile_s* file, void* buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*WriteFunction)(struct VfsFile_s* file, void* buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata);
typedef void (*CloseFunction)(struct VfsFile_s* file, VfsFunctionCallbackVoid callback, void* udata);
typedef void (*SizeFunction)(struct VfsFile_s* file, VfsFunctionCallbackSizeT callback, void* udata);

typedef struct {
    ReadFunction read;
    WriteFunction write;
    SeekFunction seek;
    TellFunction tell;
    CloseFunction close;
    SizeFunction size;
} VfsFileVtable;

typedef struct VfsFile_s {
    VFS_BASE_NODE;
    size_t block_size; // 0 means this is a character device
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
typedef void (*ReadDirFunction)(struct VfsDirectory_s* dir, VfsFunctionCallbackEntry callback, void* udata);

typedef struct {
    ReadFunction open;
    WriteFunction write;
} VfsDirectoryVtable;

typedef struct VfsDirectory_s {
    VFS_BASE_NODE;
    const VfsDirectoryVtable* functions;
} VfsDirectory;

Error initVirtualFileSystem();

// This function will take ownership over the name variable
void openNodeNamed(char* name, VfsFunctionCallbackNode callback, void* udata);

#endif
