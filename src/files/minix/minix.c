
#include "files/minix/minix.h"
#include "error/error.h"
#include "files/vfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "util/util.h"

#define MINIX_MAGIC 0x4d5a
#define BLOCK_SIZE 1024
#define NUM_IPTRS BLOCK_SIZE / 4
#define S_IFDIR 0o040000
#define S_IFREG 0o100000

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode) {
    return (2 + fs->superblock.imap_blocks + fs->superblock.zmap_blocks) * BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

typedef struct {
    const MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    uint32_t inodenum;
    VirtPtr buffer;
    size_t blocks_seen;
    size_t read;
    size_t offset;
    size_t size;
    bool write;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    MinixInode inode;
    uint16_t depth;
    uint16_t position[4];
    uint32_t zones[3][NUM_IPTRS];
} MinixOperationRequest;

static void minixGenericZoneWalkStep(MinixOperationRequest* request);

static void minixGenericFinishedCallback(Error error, size_t read, MinixOperationRequest* request) {
    if (isError(error)) {
        request->callback(error, request->read, request->udata);
        dealloc(request);
    } else {
        request->callback(simpleError(SUCCESS), request->read, request->udata);
        dealloc(request);
    }
}

static void minixGenericReadStepCallback(Error error, size_t read, MinixOperationRequest* request) {
    if (isError(error)) {
        request->callback(error, request->read, request->udata);
        dealloc(request);
    } else {
        minixGenericZoneWalkStep(request);
    }
}

static void minixOperationAtZone(MinixOperationRequest* request, size_t zone) {
    if (zone == 0) {
        // TODO: if writing and going over the end of the file, allocate new zones
        minixGenericZoneWalkStep(request);
    } else if (request->offset > BLOCK_SIZE) {
        request->blocks_seen++;
        request->offset -= BLOCK_SIZE;
        minixGenericZoneWalkStep(request);
    } else {
        request->blocks_seen++;
        size_t size = umin(BLOCK_SIZE - request->offset, request->size);
        size_t offset = zone * BLOCK_SIZE + request->offset;
        request->size -= size;
        request->read += size;
        request->offset = 0;
        if (request->write) {
            vfsWriteAt(
                request->fs->block_device, request->uid, request->gid, request->buffer, size,
                offset, (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
            );
        } else {
            vfsReadAt(
                request->fs->block_device, request->uid, request->gid, request->buffer, size,
                offset, (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
            );
        }
    }
}

static void minixGenericZoneWalkStep(MinixOperationRequest* request) {
    if (request->position[0] < 7) {
        size_t pos = request->position[0];
        request->position[0]++;
        minixOperationAtZone(request, request->inode.zones[pos]);
    } else if (request->position[0] == 7) {
        if (request->depth == 0 && request->position[1] == 0) { // We have to read level 1 zones
            request->depth = 1;
            size_t zone = request->inode.zones[7];
            if (zone == 0) {
                request->position[0]++;
                request->depth = 0;
            } else {
                size_t offset = zone * BLOCK_SIZE + request->offset;
                vfsReadAt(
                    request->fs->block_device, request->uid, request->gid,
                    virtPtrForKernel(request->zones[0]), BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            }
        } else {
            size_t pos = request->position[1];
            request->position[1]++;
            if (request->position[1] == NUM_IPTRS) {
                request->position[0]++;
                request->position[1] = 0;
                request->depth = 0;
                minixGenericZoneWalkStep(request); // Go back to the previous depth
            } else {
                minixOperationAtZone(request, request->zones[0][pos]);
            }
        }
    } else if (request->position[0] == 8) {
        if (request->depth == 0 && request->position[1] == 0) { // We have to read level 1 zones
            request->depth = 1;
            size_t zone = request->inode.zones[8];
            if (zone == 0) {
                request->position[0]++;
                request->depth = 0;
            } else {
                size_t offset = zone * BLOCK_SIZE + request->offset;
                vfsReadAt(
                    request->fs->block_device, request->uid, request->gid,
                    virtPtrForKernel(request->zones[0]), BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            }
        } else if (request->depth == 1 && request->position[2] == 0) { // We have to read level 2 zones
            size_t pos = request->position[1];
            request->position[1]++;
            if (request->position[1] == NUM_IPTRS) {
                request->position[0]++;
                request->position[1] = 0;
                request->depth = 0;
                minixGenericZoneWalkStep(request);
            } else {
                request->depth = 2;
                size_t zone = request->zones[0][pos];
                if (zone == 0) {
                    request->position[1]++;
                    request->depth = 1;
                } else {
                    size_t offset = zone * BLOCK_SIZE + request->offset;
                    vfsReadAt(
                        request->fs->block_device, request->uid, request->gid,
                        virtPtrForKernel(request->zones[1]), BLOCK_SIZE, offset,
                        (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                    );
                }
            }
        } else {
            size_t pos = request->position[2];
            request->position[2]++;
            if (request->position[2] == NUM_IPTRS) {
                request->position[1]++;
                request->position[2] = 0;
                request->depth = 1;
                minixGenericZoneWalkStep(request);
            } else {
                minixOperationAtZone(request, request->zones[1][pos]);
            }
        }
    } else if (request->position[0] == 9) {
        if (request->depth == 0 && request->position[1] == 0) { // We have to read level 1 zones
            request->depth = 1;
            size_t zone = request->inode.zones[9];
            if (zone == 0) {
                request->position[0]++;
                request->depth = 0;
            } else {
                size_t offset = zone * BLOCK_SIZE + request->offset;
                vfsReadAt(
                    request->fs->block_device, request->uid, request->gid,
                    virtPtrForKernel(request->zones[0]), BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            }
        } else if (request->depth == 1 && request->position[2] == 0) { // We have to read level 2 zones
            size_t pos = request->position[1];
            request->position[1]++;
            if (request->position[1] == NUM_IPTRS) {
                request->position[0]++;
                request->position[1] = 0;
                request->depth = 0;
                minixGenericZoneWalkStep(request);
            } else {
                request->depth = 2;
                size_t zone = request->zones[0][pos];
                if (zone == 0) {
                    request->position[1]++;
                    request->depth = 1;
                } else {
                    size_t offset = zone * BLOCK_SIZE + request->offset;
                    vfsReadAt(
                        request->fs->block_device, request->uid, request->gid,
                        virtPtrForKernel(request->zones[1]), BLOCK_SIZE, offset,
                        (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                    );
                }
            }
        } else if (request->depth == 2 && request->position[3] == 0) { // We have to read level 3 zones
            size_t pos = request->position[2];
            request->position[2]++;
            if (request->position[2] == NUM_IPTRS) {
                request->position[1]++;
                request->position[2] = 0;
                request->depth = 1;
                minixGenericZoneWalkStep(request);
            } else {
                request->depth = 3;
                size_t zone = request->zones[1][pos];
                if (zone == 0) {
                    request->position[2]++;
                    request->depth = 2;
                } else {
                    size_t offset = zone * BLOCK_SIZE + request->offset;
                    vfsReadAt(
                        request->fs->block_device, request->uid, request->gid,
                        virtPtrForKernel(request->zones[2]), BLOCK_SIZE, offset,
                        (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                    );
                }
            }
        } else {
            size_t pos = request->position[3];
            request->position[3]++;
            if (request->position[3] == NUM_IPTRS) {
                request->position[2]++;
                request->position[3] = 0;
                request->depth = 2;
                minixGenericZoneWalkStep(request);
            } else {
                minixOperationAtZone(request, request->zones[2][pos]);
            }
        }
    } else {
        request->inode.atime = getUnixTime();
        if (request->write) {
            request->inode.mtime = getUnixTime();
        }
        vfsWriteAt(
            request->fs->block_device, request->uid, request->gid,
            virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, request->inodenum),
            (VfsFunctionCallbackSizeT)minixGenericFinishedCallback, request
        );
    }
}

static void minixGenericReadINodeCallback(Error error, size_t read, MinixOperationRequest* request) {
    if (isError(error)) {
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        request->callback(simpleError(IO_ERROR), 0, request->udata);
        dealloc(request);
    } else {
        // TODO: test for user permissions
        request->position[0] = 0;
        request->position[1] = 0;
        request->position[2] = 0;
        request->position[3] = 0;
        request->depth = 0;
        minixGenericZoneWalkStep(request);
    }
}

void minixGenericOperation(
    const MinixFilesystem* fs, Uid uid, Gid gid, uint32_t inode, VirtPtr buffer, size_t length,
    size_t offset, bool write, VfsFunctionCallbackSizeT callback, void* udata
) {
    MinixOperationRequest* request = kalloc(sizeof(MinixOperationRequest));
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->inodenum = inode;
    request->buffer = buffer;
    request->offset = offset;
    request->read = 0;
    request->size = length;
    request->write = write;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        fs->block_device, uid, gid, virtPtrForKernel(&request->inode), sizeof(MinixInode),
        offsetForINode(fs, inode), (VfsFunctionCallbackSizeT)minixGenericReadINodeCallback, request
    );
}

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
        BLOCK_SIZE, (VfsFunctionCallbackSizeT)minixSuperblockReadCallback, request
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

