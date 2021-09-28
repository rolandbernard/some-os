
#include <assert.h>
#include <string.h>

#include "files/minix/file.h"

#include "files/minix/maps.h"
#include "error/log.h"
#include "error/panic.h"
#include "files/vfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "util/util.h"

typedef struct {
    MinixFile* file;
    Uid uid;
    Gid gid;
    VirtPtr buffer;
    size_t blocks_seen;
    size_t read;
    size_t offset;
    size_t size;
    size_t current_size;
    bool write;
    bool trunc;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    MinixInode inode;
    uint16_t depth;
    uint16_t position[4];
    uint32_t zones[3][MINIX_NUM_IPTRS];
    uint32_t zone_zone[3];
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
        if (request->current_size != 0) {
            request->size -= request->current_size;
            request->file->position += request->current_size;
            request->read += request->current_size;
            request->offset = 0;
            request->buffer.address += request->current_size;
        }
        minixGenericZoneWalkStep(request);
    }
}

static void minixOperationAtZone(MinixOperationRequest* request, size_t zone) {
    if (request->offset >= MINIX_BLOCK_SIZE) {
        request->blocks_seen++;
        request->offset -= MINIX_BLOCK_SIZE;
        minixGenericZoneWalkStep(request);
    } else {
        size_t size = umin(MINIX_BLOCK_SIZE - request->offset, request->size);
        size_t offset = zone * MINIX_BLOCK_SIZE + request->offset;
        request->current_size = size;
        if (request->write) {
            if (request->blocks_seen * MINIX_BLOCK_SIZE + request->offset + size > request->inode.size) {
                // Resize the file if required
                request->inode.size = request->blocks_seen * MINIX_BLOCK_SIZE + request->offset + size;
            }
            request->blocks_seen++;
            vfsWriteAt(
                request->file->fs->block_device, 0, 0, request->buffer, size,
                offset, (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
            );
        } else {
            request->blocks_seen++;
            vfsReadAt(
                request->file->fs->block_device, 0, 0, request->buffer, size,
                offset, (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
            );
        }
    }
}

static void minixWriteCurrentZone(MinixOperationRequest* request) {
    if (request->depth == 0) {
        // Will be written when writing the inode
        minixGenericZoneWalkStep(request);
    } else {
        request->current_size = 0;
        size_t parent_zone = request->zone_zone[request->depth - 1];
        size_t offset = parent_zone * MINIX_BLOCK_SIZE;
        vfsWriteAt(
            request->file->fs->block_device, 0, 0,
            virtPtrForKernel(request->zones[request->depth - 1]), MINIX_BLOCK_SIZE, offset,
            (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
        );
    }
}

static void minixGenericZeroZoneCallback(Error error, size_t written, MinixOperationRequest* request) {
    minixWriteCurrentZone(request);
}

static void minixGenericGetZoneCallback(Error error, size_t zone, MinixOperationRequest* request) {
    if (isError(error)) {
        // Unable to allocate another zone
        request->position[request->depth]++;
        minixGenericZoneWalkStep(request);
    } else {
        size_t new_zone = zone + request->file->fs->superblock.first_data_zone;
        if (request->depth == 0) {
            request->inode.zones[request->position[0]] = new_zone;
        } else {
            request->zones[request->depth - 1][request->position[request->depth]] = new_zone;
        }
        request->current_size = 0;
        static_assert(MINIX_BLOCK_SIZE <= PAGE_SIZE);
        vfsWriteAt(
            request->file->fs->block_device, 0, 0,
            virtPtrForKernel(zero_page), MINIX_BLOCK_SIZE, new_zone * MINIX_BLOCK_SIZE,
            (VfsFunctionCallbackSizeT)minixGenericZeroZoneCallback, request
        );
    }
}

static void minixGenericFreeZoneCallback(Error error, MinixOperationRequest* request) {
    if (isError(error)) {
        // Unable to allocate another zone
        request->position[request->depth]++;
        minixGenericZoneWalkStep(request);
    } else {
        if (request->depth == 0) {
            request->inode.zones[request->position[0]] = 0;
        } else {
            request->zones[request->depth - 1][request->position[request->depth]] = 0;
        }
        minixWriteCurrentZone(request);
    }
}

static void minixGenericFreeReadZoneCallback(Error error, MinixOperationRequest* request) {
    if (isError(error)) {
        // Unable to allocate another zone
        request->position[request->depth]++;
        minixGenericZoneWalkStep(request);
    } else {
        if (request->depth - 1 == 0) {
            request->inode.zones[request->position[0]] = 0;
        } else {
            request->zones[request->depth - 2][request->position[request->depth - 1]] = 0;
        }
        minixWriteCurrentZone(request);
    }
}

static void minixGenericReadFreeStepCallback(Error error, size_t read, MinixOperationRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->file->lock);
        request->callback(error, request->read, request->udata);
        dealloc(request);
    } else {
        freeMinixZone(
            request->file->fs, request->zone_zone[request->depth - 1] - request->file->fs->superblock.first_data_zone,
            (VfsFunctionCallbackVoid)minixGenericFreeReadZoneCallback, request
        );
    }
}

static void minixIndirectZoneWalkStep(size_t max_depth, MinixOperationRequest* request) {
    if (request->position[request->depth] == MINIX_NUM_IPTRS) {
        request->depth--;
        request->position[request->depth]++;
        minixGenericZoneWalkStep(request);
    } else {
        size_t zone;
        if (request->depth == 0) {
            zone = request->inode.zones[request->position[0]];
        } else {
            zone = request->zones[request->depth - 1][request->position[request->depth]];
        }
        if (request->trunc) {
            if (request->blocks_seen * MINIX_BLOCK_SIZE >= request->size) {
                // Everything else must be freed
                if (zone != 0) {
                    if (request->depth == max_depth) {
                        request->blocks_seen++;
                        freeMinixZone(
                            request->file->fs, zone - request->file->fs->superblock.first_data_zone,
                            (VfsFunctionCallbackVoid)minixGenericFreeZoneCallback, request
                        );
                    } else {
                        request->depth++;
                        request->position[request->depth] = 0;
                        request->zone_zone[request->depth - 1] = zone;
                        request->current_size = 0;
                        size_t offset = zone * MINIX_BLOCK_SIZE;
                        vfsReadAt(
                            request->file->fs->block_device, 0, 0,
                            virtPtrForKernel(request->zones[request->depth - 1]), MINIX_BLOCK_SIZE, offset,
                            (VfsFunctionCallbackSizeT)minixGenericReadFreeStepCallback, request
                        );
                    }
                    return;
                }
            } else {
                // More space needed
                if (zone == 0) {
                    if (request->blocks_seen * MINIX_BLOCK_SIZE >= request->inode.size) {
                        getFreeMinixZone(
                            request->file->fs, (VfsFunctionCallbackSizeT)minixGenericGetZoneCallback, request
                        );
                        return;
                    }
                } else if (request->depth == max_depth) {
                    request->blocks_seen++;
                }
            }
            if (request->depth != max_depth && zone != 0) {
                request->depth++;
                request->zone_zone[request->depth - 1] = zone;
                request->position[request->depth] = 0;
                request->current_size = 0;
                size_t offset = zone * MINIX_BLOCK_SIZE;
                vfsReadAt(
                    request->file->fs->block_device, 0, 0,
                    virtPtrForKernel(request->zones[request->depth - 1]), MINIX_BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            } else {
                request->position[request->depth]++;
                minixGenericZoneWalkStep(request);
            }
        } else if (zone == 0) {
            // If writing and going over the end of the file, allocate new zones
            if (request->write && request->blocks_seen * MINIX_BLOCK_SIZE >= request->inode.size) {
                getFreeMinixZone(
                    request->file->fs, (VfsFunctionCallbackSizeT)minixGenericGetZoneCallback, request
                );
            } else {
                request->position[request->depth]++;
                minixGenericZoneWalkStep(request);
            }
        } else {
            if (request->depth == max_depth) {
                request->position[request->depth]++;
                minixOperationAtZone(request, zone);
            } else {
                request->depth++;
                request->zone_zone[request->depth - 1] = zone;
                request->position[request->depth] = 0;
                request->current_size = 0;
                size_t offset = zone * MINIX_BLOCK_SIZE;
                vfsReadAt(
                    request->file->fs->block_device, 0, 0,
                    virtPtrForKernel(request->zones[request->depth - 1]), MINIX_BLOCK_SIZE, offset,
                    (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
                );
            }
        }
    }
}

static void minixGenericZoneWalkStep(MinixOperationRequest* request) {
    if ((!request->trunc && request->size == 0) || request->position[0] > 9) {
        request->inode.atime = getUnixTime();
        if (request->trunc) {
            request->inode.size = request->size;
        }
        if (request->write || request->trunc) {
            request->inode.mtime = getUnixTime();
        }
        vfsWriteAt(
            request->file->fs->block_device, 0, 0,
            virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->file->fs, request->file->inodenum),
            (VfsFunctionCallbackSizeT)minixGenericFinishedCallback, request
        );
    } else if (request->position[0] < 7) {
        minixIndirectZoneWalkStep(0, request);
    } else if (request->position[0] == 7) {
        minixIndirectZoneWalkStep(1, request);
    } else if (request->position[0] == 8) {
        minixIndirectZoneWalkStep(2, request);
    } else if (request->position[0] == 9) {
        minixIndirectZoneWalkStep(3, request);
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
    } else if (request->write && !canAccess(request->inode.mode, request->inode.uid, request->inode.gid, request->uid, request->gid, VFS_ACCESS_W)) {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(FORBIDDEN), 0, request->udata);
        dealloc(request);
    } else if (!request->write && !canAccess(request->inode.mode, request->inode.uid, request->inode.gid, request->uid, request->gid, VFS_ACCESS_R)) {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(FORBIDDEN), 0, request->udata);
        dealloc(request);
    } else {
        request->position[0] = 0;
        request->position[1] = 0;
        request->position[2] = 0;
        request->position[3] = 0;
        request->depth = 0;
        request->blocks_seen = 0;
        if (request->inode.size < request->offset + request->size && !request->write && !request->trunc) {
            // Can't read past the end of the file
            if (request->inode.size < request->offset) {
                request->size = 0;
            } else {
                request->size = request->inode.size - request->offset;
            }
        }
        minixGenericZoneWalkStep(request);
    }
}

static void minixGenericOperation(
    MinixFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t length, bool write, bool trunc,
    VfsFunctionCallbackSizeT callback, void* udata
) {
    MinixOperationRequest* request = kalloc(sizeof(MinixOperationRequest));
    lockSpinLock(&file->lock);
    request->file = file;
    request->uid = uid;
    request->gid = gid;
    request->buffer = buffer;
    request->offset = file->position;
    request->read = 0;
    request->size = length;
    request->write = write;
    request->trunc = trunc;
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
    minixGenericOperation(file, uid, gid, buffer, size, false, false, callback, udata);
}

static void minixWriteFunction(
    MinixFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t size,
    VfsFunctionCallbackSizeT callback, void* udata
) {
    minixGenericOperation(file, uid, gid, buffer, size, true, false, callback, udata);
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
        file->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
        offsetForINode(file->fs, file->inodenum), (VfsFunctionCallbackSizeT)minixStatINodeCallback,
        request
    );
}

static void minixCloseFunction(
    MinixFile* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata
) {
    lockSpinLock(&file->lock);
    file->fs->base.open_files--;
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

typedef struct {
    VfsFunctionCallbackVoid callback;
    void* udata;
} MinixTruncRequest;

static void minixTruncCallback(Error error, size_t ignore, MinixTruncRequest* request) {
    request->callback(error, request->udata);
    dealloc(request);
}

static void minixTruncFunction(MinixFile* file, Uid uid, Gid gid, size_t size, VfsFunctionCallbackVoid callback, void* udata) {
    MinixTruncRequest* request = kalloc(sizeof(MinixStatRequest));
    request->callback = callback;
    request->udata = udata;
    minixGenericOperation(
        file, uid, gid, virtPtrForKernel(NULL), size, false, true,
        (VfsFunctionCallbackSizeT)minixTruncCallback, request
    );
}

typedef struct {
    MinixFile* file;
    Uid uid;
    VfsMode mode;
    Uid new_uid;
    Gid new_gid;
    bool chown;
    VfsFunctionCallbackVoid callback;
    void* udata;
    MinixInode inode;
} MinixChRequest;

static void minixChWriteCallback(Error error, size_t read, MinixChRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->file->lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(IO_ERROR), request->udata);
        dealloc(request);
    } else {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(SUCCESS), request->udata);
        dealloc(request);
    }
}

static void minixChReadCallback(Error error, size_t read, MinixChRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->file->lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(IO_ERROR), request->udata);
        dealloc(request);
    } else if (request->uid != 0 && request->uid != request->inode.uid) {
        unlockSpinLock(&request->file->lock);
        request->callback(simpleError(FORBIDDEN), request->udata);
        dealloc(request);
    } else {
        if (request->chown) {
            request->inode.uid = request->new_uid;
            request->inode.gid = request->new_gid;
        } else {
            request->inode.mode &= VFS_MODE_TYPE; // Clear everything but the type
            request->inode.mode |= request->mode & ~VFS_MODE_TYPE; // Don't allow changing the type
        }
        vfsWriteAt(
            request->file->fs->block_device, 0, 0, virtPtrForKernel(&request->inode),
            sizeof(MinixInode), offsetForINode(request->file->fs, request->file->inodenum),
            (VfsFunctionCallbackSizeT)minixChWriteCallback, request
        );
    }
}

static void minixChFunction(
    MinixFile* file, Uid uid, Gid gid, VfsMode mode, Uid new_uid, Gid new_gid, bool chown,
    VfsFunctionCallbackVoid callback, void* udata
) {
    MinixChRequest* request = kalloc(sizeof(MinixChRequest));
    lockSpinLock(&file->lock);
    request->file = file;
    request->uid = uid;
    request->new_uid = new_uid;
    request->new_gid = new_gid;
    request->chown = chown;
    request->mode = mode;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        file->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
        offsetForINode(file->fs, file->inodenum),
        (VfsFunctionCallbackSizeT)minixChReadCallback, request
    );
}

static void minixChmodFunction(MinixFile* file, Uid uid, Gid gid, VfsMode mode, VfsFunctionCallbackVoid callback, void* udata) {
    minixChFunction(file, uid, gid, mode, 0, 0, false, callback, udata);
}

static void minixChownFunction(MinixFile* file, Uid uid, Gid gid, Uid new_uid, Gid new_gid, VfsFunctionCallbackVoid callback, void* udata) {
    minixChFunction(file, uid, gid, 0, new_uid, new_gid, true, callback, udata);
}

typedef struct {
    MinixFile* file;
    Uid uid;
    Gid gid;
    VirtPtr buff;
    size_t size;
    size_t position;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    MinixDirEntry entry;
} MinixReaddirRequest;

static void minixReaddirReadCallback(Error error, size_t read, void* udata) {
    MinixReaddirRequest* request = (MinixReaddirRequest*)udata;
    if (isError(error)) {
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != 0 && read != sizeof(MinixDirEntry)) {
        request->callback(simpleError(IO_ERROR), 0, request->udata);
        dealloc(request);
    } else {
        if (read == 0) {
            request->callback(simpleError(SUCCESS), 0, request->udata); // Didn't read anything
        } else {
            size_t name_len = strlen((char*)request->entry.name);
            size_t size = name_len + 1 + sizeof(VfsDirectoryEntry);
            VfsDirectoryEntry* entry = kalloc(size);
            entry->id = request->entry.inode;
            entry->off = request->position;
            entry->len = size;
            entry->type = VFS_TYPE_UNKNOWN;
            memcpy(entry->name, request->entry.name, name_len + 1);
            memcpyBetweenVirtPtr(request->buff, virtPtrForKernel(entry), umin(request->size, size));
            request->callback(simpleError(SUCCESS), umin(request->size, size), request->udata);
        }
        dealloc(request);
    }
}

static void minixReaddirFunction(MinixFile* file, Uid uid, Gid gid, VirtPtr buff, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    MinixReaddirRequest* request = kalloc(sizeof(MinixReaddirRequest));
    request->file = file;
    request->uid = uid;
    request->gid = gid;
    request->buff = buff;
    request->size = size;
    request->callback = callback;
    request->udata = udata;
    request->position = file->position;
    file->base.functions->read(
        (VfsFile*)file, uid, gid, virtPtrForKernel(&request->entry), sizeof(MinixDirEntry),
        minixReaddirReadCallback, request
    );
}

static VfsFileVtable functions_file = {
    .seek = (SeekFunction)minixSeekFunction,
    .read = (ReadFunction)minixReadFunction,
    .write = (WriteFunction)minixWriteFunction,
    .stat = (StatFunction)minixStatFunction,
    .close = (CloseFunction)minixCloseFunction,
    .dup = (DupFunction)minixDupFunction,
    .trunc = (TruncFunction)minixTruncFunction,
    .chmod = (ChmodFunction)minixChmodFunction,
    .chown = (ChownFunction)minixChownFunction,
};

static VfsFileVtable functions_directory = {
    .seek = (SeekFunction)minixSeekFunction,
    .read = (ReadFunction)minixReadFunction,
    .write = (WriteFunction)minixWriteFunction,
    .stat = (StatFunction)minixStatFunction,
    .close = (CloseFunction)minixCloseFunction,
    .dup = (DupFunction)minixDupFunction,
    .trunc = (TruncFunction)minixTruncFunction,
    .chmod = (ChmodFunction)minixChmodFunction,
    .chown = (ChownFunction)minixChownFunction,
    .readdir = (ReaddirFunction)minixReaddirFunction,
};

MinixFile* createMinixFileForINode(MinixFilesystem* fs, uint32_t inode, bool dir) {
    fs->base.open_files++;
    MinixFile* file = zalloc(sizeof(MinixFile));
    if (dir) {
        file->base.functions = &functions_directory;
    } else {
        file->base.functions = &functions_file;
    }
    file->fs = fs;
    file->inodenum = inode;
    file->position = 0;
    return file;
}

