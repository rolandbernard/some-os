
#include <string.h>

#include "files/minix/file.h"

#include "error/panic.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "util/util.h"

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode) {
    return (2 + fs->superblock.imap_blocks + fs->superblock.zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

typedef struct {
    MinixFile* file;
    Uid uid;
    Gid gid;
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
    uint32_t zones[3][MINIX_NUM_IPTRS];
} MinixOperationRequest;

static void minixGenericZoneWalkStep(MinixOperationRequest* request);

static void minixGenericFinishedCallback(Error error, size_t read, MinixOperationRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->file->lock);
        request->callback(error, request->read, request->udata);
        dealloc(request);
    } else {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(SUCCESS), request->read, request->udata);
        dealloc(request);
    }
}

static void minixGenericReadStepCallback(Error error, size_t read, MinixOperationRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->file->lock);
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
        request->file->position += size;
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
        } else if (request->depth == 1 && request->position[2] == 0) { // We have to read level 2
                                                                       // zones
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
        } else if (request->depth == 1 && request->position[2] == 0) { // We have to read level 2
                                                                       // zones
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
        } else if (request->depth == 2 && request->position[3] == 0) { // We have to read level 3
                                                                       // zones
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
        unlockSpinLock(&request->file->lock);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockSpinLock(&request->file->lock);
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
    MinixFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t length, bool write,
    VfsFunctionCallbackSizeT callback, void* udata
) {
    MinixOperationRequest* request = kalloc(sizeof(MinixOperationRequest));
    request->file = file;
    request->uid = uid;
    request->gid = gid;
    request->buffer = buffer;
    request->offset = file->position;
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

typedef struct {
    MinixFile* file;
    size_t offset;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    MinixInode inode;
} MinixSeekRequest;

static void minixSeekINodeCallback(Error error, size_t read, MinixSeekRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->file->lock);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(IO_ERROR), 0, request->udata);
        dealloc(request);
    } else {
        size_t new_position = request->inode.size + request->offset; // This is only used for SEEK_END
        request->file->position = new_position;
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(SUCCESS), new_position, request->udata);
        dealloc(request);
    }
}

static void minixSeekFunction(MinixFile* file, Uid uid, Gid gid, size_t offset, VfsSeekWhence whence, VfsFunctionCallbackSizeT callback, void* udata) {
    if (whence == VFS_SEEK_END) { // We have to read the size to know where the end is
        MinixSeekRequest* request = kalloc(sizeof(MinixSeekRequest));
        lockSpinLock(&file->lock);
        request->file = file;
        request->offset = offset;
        request->callback = callback;
        request->udata = udata;
        vfsReadAt(
            file->fs->block_device, uid, gid, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(file->fs, file->inodenum),
            (VfsFunctionCallbackSizeT)minixSeekINodeCallback, request
        );
    } else {
        lockSpinLock(&file->lock);
        size_t new_position;
        switch (whence) {
            case VFS_SEEK_CUR:
                new_position = file->position + offset;
                break;
            case VFS_SEEK_SET:
                new_position = offset;
                break;
            case VFS_SEEK_END:
                panic();
        }
        file->position = new_position;
        unlockSpinLock(&file->lock);
        callback(simpleError(SUCCESS), new_position, udata);
    }
}

static void minixReadFunction(
    MinixFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t size,
    VfsFunctionCallbackSizeT callback, void* udata
) {
    minixGenericOperation(file, uid, gid, buffer, size, false, callback, udata);
}

static void minixWriteFunction(
    MinixFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t size,
    VfsFunctionCallbackSizeT callback, void* udata
) {
    minixGenericOperation(file, uid, gid, buffer, size, true, callback, udata);
}

typedef struct {
    MinixFile* file;
    VfsFunctionCallbackStat callback;
    void* udata;
    MinixInode inode;
} MinixStatRequest;

static void minixStatINodeCallback(Error error, size_t read, MinixStatRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->file->lock);
        VfsStat ret = {};
        request->callback(error, ret, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockSpinLock(&request->file->lock);
        VfsStat ret = {};
        request->callback(simpleError(IO_ERROR), ret, request->udata);
        dealloc(request);
    } else {
        unlockSpinLock(&request->file->lock);
        VfsStat ret = {
            .id = request->file->inodenum,
            .mode = request->inode.mode,
            .nlinks = request->inode.nlinks,
            .uid = request->inode.uid,
            .gid = request->inode.gid,
            .size = request->inode.size,
            .block_size = 1,
            .st_atime = request->inode.atime,
            .st_mtime = request->inode.mtime,
            .st_ctime = request->inode.ctime,
        };
        request->callback(simpleError(SUCCESS), ret, request->udata);
    }
}

static void minixStatFunction(
    MinixFile* file, Uid uid, Gid gid, VfsFunctionCallbackStat callback, void* udata
) {
    MinixStatRequest* request = kalloc(sizeof(MinixStatRequest));
    lockSpinLock(&file->lock);
    request->file = file;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        file->fs->block_device, uid, gid, virtPtrForKernel(&request->inode), sizeof(MinixInode),
        offsetForINode(file->fs, file->inodenum), (VfsFunctionCallbackSizeT)minixStatINodeCallback,
        request
    );
}

static void minixCloseFunction(
    MinixFile* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata
) {
    lockSpinLock(&file->lock);
    dealloc(file);
    callback(simpleError(SUCCESS), udata);
}

static void minixDupFunction(MinixFile* file, Uid uid, Gid gid, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&file->lock);
    MinixFile* copy = kalloc(sizeof(MinixFile));
    memcpy(copy, file, sizeof(MinixFile));
    unlockSpinLock(&file->lock);
    unlockSpinLock(&copy->lock); // Also unlock the new file
    callback(simpleError(SUCCESS), (VfsFile*)copy, udata);
}

static VfsFileVtable functions = {
    .seek = (SeekFunction)minixSeekFunction,
    .read = (ReadFunction)minixReadFunction,
    .write = (WriteFunction)minixWriteFunction,
    .stat = (StatFunction)minixStatFunction,
    .close = (CloseFunction)minixCloseFunction,
    .dup = (DupFunction)minixDupFunction,
};

MinixFile* createMinixFileForINode(const MinixFilesystem* fs, uint32_t inode) {
    MinixFile* file = zalloc(sizeof(MinixFile));
    file->base.functions = &functions;
    file->fs = fs;
    file->inodenum = inode;
    file->position = 0;
    return file;
}
