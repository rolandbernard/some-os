
#include "files/minix/file.h"

#include "kernel/time.h"
#include "memory/kalloc.h"
#include "util/util.h"

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode) {
    return (2 + fs->superblock.imap_blocks + fs->superblock.zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

typedef struct {
    const MinixFile* file;
    Uid uid;
    Gid gid;
    VirtPtr buffer;
    size_t blocks_seen;
    size_t read;
    size_t offset;
    size_t size;
    bool write;
    MinixInode inode;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    uint16_t depth;
    uint16_t position[4];
    uint32_t zones[3][MINIX_NUM_IPTRS];
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
    } else if (request->offset > MINIX_BLOCK_SIZE) {
        request->blocks_seen++;
        request->offset -= MINIX_BLOCK_SIZE;
        minixGenericZoneWalkStep(request);
    } else {
        request->blocks_seen++;
        size_t size = umin(MINIX_BLOCK_SIZE - request->offset, request->size);
        size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
        request->size -= size;
        request->read += size;
        request->offset = 0;
        if (request->write) {
            vfsWriteAt(
                request->file->fs->block_device, request->uid, request->gid, request->buffer, size,
                offset, (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
            );
        } else {
            vfsReadAt(
                request->file->fs->block_device, request->uid, request->gid, request->buffer, size,
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
                size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
                vfsReadAt(
                    request->file->fs->block_device, request->uid, request->gid,
                    virtPtrForKernel(request->zones[0]), MINIX_BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            }
        } else {
            size_t pos = request->position[1];
            request->position[1]++;
            if (request->position[1] == MINIX_NUM_IPTRS) {
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
                size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
                vfsReadAt(
                    request->file->fs->block_device, request->uid, request->gid,
                    virtPtrForKernel(request->zones[0]), MINIX_BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            }
        } else if (request->depth == 1 && request->position[2] == 0) { // We have to read level 2 zones
            size_t pos = request->position[1];
            request->position[1]++;
            if (request->position[1] == MINIX_NUM_IPTRS) {
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
                    size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
                    vfsReadAt(
                        request->file->fs->block_device, request->uid, request->gid,
                        virtPtrForKernel(request->zones[1]), MINIX_BLOCK_SIZE, offset,
                        (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                    );
                }
            }
        } else {
            size_t pos = request->position[2];
            request->position[2]++;
            if (request->position[2] == MINIX_NUM_IPTRS) {
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
                size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
                vfsReadAt(
                    request->file->fs->block_device, request->uid, request->gid,
                    virtPtrForKernel(request->zones[0]), MINIX_BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            }
        } else if (request->depth == 1 && request->position[2] == 0) { // We have to read level 2 zones
            size_t pos = request->position[1];
            request->position[1]++;
            if (request->position[1] == MINIX_NUM_IPTRS) {
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
                    size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
                    vfsReadAt(
                        request->file->fs->block_device, request->uid, request->gid,
                        virtPtrForKernel(request->zones[1]), MINIX_BLOCK_SIZE, offset,
                        (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                    );
                }
            }
        } else if (request->depth == 2 && request->position[3] == 0) { // We have to read level 3 zones
            size_t pos = request->position[2];
            request->position[2]++;
            if (request->position[2] == MINIX_NUM_IPTRS) {
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
                    size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
                    vfsReadAt(
                        request->file->fs->block_device, request->uid, request->gid,
                        virtPtrForKernel(request->zones[2]), MINIX_BLOCK_SIZE, offset,
                        (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                    );
                }
            }
        } else {
            size_t pos = request->position[3];
            request->position[3]++;
            if (request->position[3] == MINIX_NUM_IPTRS) {
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
            request->file->fs->block_device, request->uid, request->gid,
            virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->file->fs, request->file->inodenum),
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
        // TODO: test for user permissions (Maybe only on open?)
        request->position[0] = 0;
        request->position[1] = 0;
        request->position[2] = 0;
        request->position[3] = 0;
        request->depth = 0;
        minixGenericZoneWalkStep(request);
    }
}

static void minixGenericOperation(
    const MinixFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t length, size_t offset,
    bool write, VfsFunctionCallbackSizeT callback, void* udata
) {
    MinixOperationRequest* request = kalloc(sizeof(MinixOperationRequest));
    request->file = file;
    request->uid = uid;
    request->gid = gid;
    request->buffer = buffer;
    request->offset = offset;
    request->read = 0;
    request->size = length;
    request->write = write;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        file->fs->block_device, uid, gid, virtPtrForKernel(&request->inode), sizeof(MinixInode),
        offsetForINode(file->fs, file->inodenum),
        (VfsFunctionCallbackSizeT)minixGenericReadINodeCallback, request
    );
}

static VfsFileVtable functions = {
};

MinixFile* createMinixFileForINode(const MinixFilesystem* fs, uint32_t inode) {
    MinixFile* file = zalloc(sizeof(MinixFile));
    file->base.functions = &functions;
    file->fs = fs;
    file->inodenum = inode;
    file->read_position = 0;
    return file;
}

