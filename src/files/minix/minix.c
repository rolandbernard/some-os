
#include <stddef.h>
#include <string.h>

#include "files/minix/minix.h"
#include "files/minix/file.h"

#include "memory/kalloc.h"
#include "memory/virtptr.h"

typedef void (*MinixINodeCallback)(Error error, uint32_t inode, void* udata);

typedef struct {
    MinixFile* file;
    Uid uid;
    Gid gid;
    const char* name;
    MinixINodeCallback callback;
    void* udata;
    size_t size;
    size_t entry_count;
    MinixDirEntry* entries;
} MinixFindInDirectoryRequest;

#define MAX_SINGLE_READ_SIZE (1 << 16)

static void minixFindINodeInReadCallback(Error error, size_t read, MinixFindInDirectoryRequest* request);

static void readEntriesForRequest(MinixFindInDirectoryRequest* request) {
    size_t size = request->size;
    if (size < sizeof(MinixDirEntry)) {
        dealloc(request->entries);
        request->callback(simpleError(NO_SUCH_FILE), 0, request->udata);
        dealloc(request);
    } else {
        if (size > MAX_SINGLE_READ_SIZE) {
            size = MAX_SINGLE_READ_SIZE;
        }
        request->size -= size;
        if (request->entries == NULL) {
            request->entries = kalloc(size);
        }
        request->entry_count = size / sizeof(MinixDirEntry);
        request->file->base.functions->read(
            (VfsFile*)request->file, request->uid, request->gid, virtPtrForKernel(request->entries),
            size, (VfsFunctionCallbackSizeT)minixFindINodeInReadCallback, request
        );
    }
}

static void minixFindINodeInReadCallback(Error error, size_t read, MinixFindInDirectoryRequest* request) {
    if (isError(error)) {
        dealloc(request->entries);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read == 0) {
        dealloc(request->entries);
        request->callback(simpleError(NO_SUCH_FILE), 0, request->udata);
        dealloc(request);
    } else {
        for (size_t i = 0; i < request->entry_count; i++) {
            if (strcmp((char*)request->entries[i].name, request->name) == 0) {
                uint32_t inode = request->entries[i].inode;
                dealloc(request->entries);
                request->callback(simpleError(SUCCESS), inode, request->udata);
                dealloc(request);
                return;
            }
        }
        readEntriesForRequest(request);
    }
}

static void minixFindINodeInStatCallback(Error error, VfsStat stat, MinixFindInDirectoryRequest* request) {
    if (isError(error)) {
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (MODE_TYPE(stat.mode) != VFS_TYPE_DIR) {
        // File is not a directory
        request->callback(simpleError(WRONG_FILE_TYPE), 0, request->udata);
        dealloc(request);
    } else {
        request->size = stat.size;
        readEntriesForRequest(request);
    }
}

static void minixFindINodeForNameIn(MinixFile* file, Uid uid, Gid gid, const char* name, MinixINodeCallback callback, void* udata) {
    MinixFindInDirectoryRequest* request = kalloc(sizeof(MinixFindInDirectoryRequest));
    request->file = file;
    request->uid = uid;
    request->gid = gid;
    request->name = name;
    request->callback = callback;
    request->udata = udata;
    file->base.functions->stat((VfsFile*)file, uid, gid, (VfsFunctionCallbackStat)minixFindINodeInStatCallback, request);
}

typedef struct {
    const MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    const char* path;
    char* path_copy;
    MinixINodeCallback callback;
    void* udata;
} MinixFindINodeRequest;

static void minixFindINodeForPath(const MinixFilesystem* fs, Uid uid, Gid gid, const char* path, MinixINodeCallback callback, void* udata) {
    
}

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    VfsFunctionCallbackFile callback;
    void* udata;
} MinixOpenRequest;

static void minixOpenINodeCallback(Error error, uint32_t inode, void* udata) {
}

static void minixOpenFunction(
    const MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode,
    VfsFunctionCallbackFile callback, void* udata
) {
}

static void minixUnlinkFunction(const MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
}

static void minixLinkFunction(const MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
}

static void minixRenameFunction(const MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
}

static void minixFreeFunction(MinixFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    // We don't do any caching. Otherwise write it to disk here.
    dealloc(fs);
    callback(simpleError(SUCCESS), udata);
}

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    VfsFunctionCallbackVoid callback;
    void* udata;
} MinixInitRequest;

static void minixSuperblockReadCallback(Error error, size_t read, MinixInitRequest* request) {
    if (isError(error)) {
        request->callback(error, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixSuperblock)) {
        request->callback(simpleError(IO_ERROR), request->udata);
        dealloc(request);
    } else if (request->fs->superblock.magic != MINIX_MAGIC) {
        request->callback(simpleError(WRONG_FILE_TYPE), request->udata);
        dealloc(request);
    } else {
        // Don't read more for now
        request->callback(simpleError(SUCCESS), request->udata);
        dealloc(request);
    }
}

static void minixInitFunction(MinixFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    MinixInitRequest* request = kalloc(sizeof(MinixInitRequest));
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        fs->block_device, uid, gid, virtPtrForKernel(&fs->superblock), sizeof(MinixSuperblock),
        MINIX_BLOCK_SIZE, (VfsFunctionCallbackSizeT)minixSuperblockReadCallback, request
    );
}

static const VfsFilesystemVtable functions = {
    .open = (OpenFunction)minixOpenFunction,
    .unlink = (UnlinkFunction)minixUnlinkFunction,
    .link = (LinkFunction)minixLinkFunction,
    .rename = (RenameFunction)minixRenameFunction,
    .free = (FreeFunction)minixFreeFunction,
    .init = (InitFunction)minixInitFunction,
};

MinixFilesystem* createMinixFilesystem(VfsFile* block_device, void* data) {
    MinixFilesystem* file = zalloc(sizeof(MinixFilesystem));
    file->base.functions = &functions;
    file->block_device = block_device;
    return file;
}

