
#include "files/minix3/minix.h"
#include "error/error.h"
#include "memory/kalloc.h"

#define MINIX_MAGIC 0x4d5a
#define BLOCK_SIZE 1024
#define NUM_IPTRS BLOCK_SIZE / 4
#define S_IFDIR 0o040000
#define S_IFREG 0o100000

typedef void (*MinixINodeCallback)(Error error, uint32_t inode, void* udata);

static void minixFindINodeForPath(MinixFilesystem* fs, const char* path, MinixINodeCallback callback, void* udata) {
}

static void minixOpenFunction(
    MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode,
    VfsFunctionCallbackFile callback, void* udata
) {
}

static void minixUnlinkFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
}

static void minixLinkFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
}

static void minixRenameFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
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
    size_t read;
    VfsFunctionCallbackVoid callback;
    void* udata;
} MinixInitRequest;

static void minixSuperblockReadCallback(Error error, size_t read, MinixInitRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else if (read == 0) {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(IO_ERROR), request->udata);
        dealloc(request);
    } else if (request->read + read != sizeof(MinixSuperblock)) {
        // Try to read the remaining bytes
        request->read += read;
        request->fs->block_device->functions->read(
            request->fs->block_device, request->uid, request->gid,
            virtPtrForKernel((void*)&request->fs->superblock + request->read),
            sizeof(MinixSuperblock) - request->read,
            (VfsFunctionCallbackSizeT)minixSuperblockReadCallback, request
        );
    } else if (request->fs->superblock.magic != MINIX_MAGIC) {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(WRONG_FILE_TYPE), request->udata);
        dealloc(request);
    } else {
        // Don't read more for now
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(SUCCESS), request->udata);
        dealloc(request);
    }
}

static void minixSuperblockSeekCallback(Error error, size_t offset, MinixInitRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else if (offset != 1024) {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(IO_ERROR), request->udata);
        dealloc(request);
    } else {
        request->read = 0;
        request->fs->block_device->functions->read(
            request->fs->block_device, request->uid, request->gid,
            virtPtrForKernel(&request->fs->superblock), sizeof(MinixSuperblock),
            (VfsFunctionCallbackSizeT)minixSuperblockReadCallback, request
        );
    }
}

static void minixInitFunction(MinixFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    MinixInitRequest* request = kalloc(sizeof(MinixInitRequest));
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->callback = callback;
    request->udata = udata;
    lockSpinLock(&fs->lock);
    fs->block_device->functions->seek(
        fs->block_device, request->uid, request->gid, 1024, VFS_SEEK_SET,
        (VfsFunctionCallbackSizeT)minixSuperblockSeekCallback, request
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

