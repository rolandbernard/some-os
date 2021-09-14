
#include <stddef.h>

#include "files/minix/minix.h"

#include "memory/kalloc.h"
#include "memory/virtptr.h"

typedef void (*MinixINodeCallback)(Error error, uint32_t inode, void* udata);

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    size_t read;
    VfsFunctionCallbackVoid callback;
    void* udata;
} MinixFindINodeRequest;

static void minixFindINodeForPath(const MinixFilesystem* fs, const char* path, MinixINodeCallback callback, void* udata) {
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

