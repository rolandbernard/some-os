
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
    Process* process;
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
        unlockTaskLock(&request->file->lock);
        request->callback(error, request->read, request->udata);
        dealloc(request);
    } else {
        unlockTaskLock(&request->file->lock);
        request->callback(simpleError(SUCCESS), request->read, request->udata);
        dealloc(request);
    }
}

static void minixGenericReadStepCallback(Error error, size_t read, MinixOperationRequest* request) {
    if (isError(error)) {
        unlockTaskLock(&request->file->lock);
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
                request->file->fs->block_device, NULL, request->buffer, size,
                offset, (VfsFunctionCallbackSizeT)minixGenericReadStepCallback, request
            );
        } else {
            request->blocks_seen++;
            vfsReadAt(
                request->file->fs->block_device, NULL, request->buffer, size,
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
            request->file->fs->block_device, NULL,
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
            request->file->fs->block_device, NULL, virtPtrForKernel(zero_page), MINIX_BLOCK_SIZE,
            new_zone * MINIX_BLOCK_SIZE, (VfsFunctionCallbackSizeT)minixGenericZeroZoneCallback,
            request
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
        unlockTaskLock(&request->file->lock);
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
                            request->file->fs->block_device, NULL,
                            virtPtrForKernel(request->zones[request->depth - 1]), MINIX_BLOCK_SIZE,
                            offset, (VfsFunctionCallbackSizeT)minixGenericReadFreeStepCallback,
                            request
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
                    request->file->fs->block_device, NULL,
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
                    request->file->fs->block_device, NULL,
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
            request->file->fs->block_device, NULL, virtPtrForKernel(&request->inode),
            sizeof(MinixInode), offsetForINode(request->file->fs, request->file->inodenum),
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
        unlockTaskLock(&request->file->lock);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockTaskLock(&request->file->lock);
        request->callback(simpleError(EIO), 0, request->udata);
        dealloc(request);
    } else if (
        request->write
        && !canAccess(
            request->inode.mode, request->inode.uid, request->inode.gid, request->process, VFS_ACCESS_W
        )
    ) {
        unlockTaskLock(&request->file->lock);
        request->callback(simpleError(EACCES), 0, request->udata);
        dealloc(request);
    } else if (
        !request->write
        && !canAccess(
            request->inode.mode, request->inode.uid, request->inode.gid, request->process, VFS_ACCESS_R
        )
    ) {
        unlockTaskLock(&request->file->lock);
        request->callback(simpleError(EACCES), 0, request->udata);
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

typedef Error (*MinixZoneWalkFunction)(uint32_t* zone, size_t position, size_t size, bool pre, bool post, void* udata);

static Error minixZoneWalkRec(
    MinixFile* file, size_t* position, size_t offset, size_t depth, uint32_t indirect_table, MinixZoneWalkFunction callback, void* udata
);

static Error minixZoneWalkRecScan(
    MinixFile* file, size_t* position, size_t offset, size_t depth, uint32_t* table, size_t len, MinixZoneWalkFunction callback, void* udata
) {
    for (size_t i = 0; i < len; i++) {
        size_t size = MINIX_BLOCK_SIZE << (MINIX_NUM_IPTRS_LOG2 * depth);
        if (offset < *position + size) {
            CHECKED(callback(table + i, *position, size, true, false, udata));
            if (depth == 0 || table[i] == 0) {
                CHECKED(callback(table + i, *position, size, false, false, udata));
            } else {
                CHECKED(minixZoneWalkRec(file, position, offset, depth - 1, table[i], callback, udata));
            }
            CHECKED(callback(table + i, *position, size, false, true, udata));
        }
        *position += size;
    }
    return simpleError(SUCCESS);
}

static Error minixZoneWalkRec(
    MinixFile* file, size_t* position, size_t offset, size_t depth, uint32_t indirect_table, MinixZoneWalkFunction callback, void* udata
) {
    uint32_t* table = kalloc(MINIX_BLOCK_SIZE);
    size_t tmp_size;
    CHECKED(vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(table), MINIX_BLOCK_SIZE, offsetForZone(indirect_table), &tmp_size
    ), dealloc(table));
    if (tmp_size != MINIX_BLOCK_SIZE) {
        dealloc(table);
        return simpleError(EIO);
    }
    CHECKED(minixZoneWalkRecScan(file, position, offset, depth, table, MINIX_NUM_IPTRS, callback, udata), dealloc(table));
    dealloc(table);
    return simpleError(SUCCESS);
}

static Error minixZoneWalk(
    MinixFile* file, size_t offset, MinixInode* inode, MinixZoneWalkFunction callback, void* udata
) {
    size_t position = 0;
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 0, inode->zones, 7, callback, udata));
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 1, inode->zones + 7, 1, callback, udata));
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 2, inode->zones + 8, 1, callback, udata));
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 3, inode->zones + 9, 1, callback, udata));
    return simpleError(SUCCESS);
}

typedef struct {
    MinixFile* file;
    Process* process;
    VirtPtr buffer;
    size_t length;
    size_t left;
    bool write;
} MinixReadWriteRequest;

static Error minixGenOpZoneWalkCallback(uint32_t* zone, size_t position, size_t size, bool pre, bool post, void* udata) {
    MinixReadWriteRequest* request = (MinixReadWriteRequest*)udata;
    if (request->write) {
        if (pre) {
            if (*zone == 0) {
                size_t new_zone;
                CHECKED(getFreeMinixZone(request->file->fs, &new_zone));
                assert(MINIX_BLOCK_SIZE < PAGE_SIZE);
                size_t tmp_size;
                CHECKED(vfsWriteAt(
                    request->file->fs->block_device, NULL, virtPtrForKernel(zero_page), MINIX_BLOCK_SIZE, offsetForZone(new_zone), &tmp_size
                ));
                if (tmp_size != MINIX_BLOCK_SIZE) {
                    return simpleError(EIO);
                }
                *zone = new_zone;
            }
        }
    }
    if (!pre && !post && request->left > 0) {
        size_t file_offset =
            request->file->position > position ? request->file->position : position;
        size_t block_offset = file_offset % MINIX_BLOCK_SIZE;
        size_t tmp_size = umin(request->left, position + size - file_offset);
        if (request->write) {
            CHECKED(vfsWriteAt(
                request->file->fs->block_device, NULL, request->buffer, tmp_size, offsetForZone(*zone) + block_offset, &tmp_size
            ));
        } else if (*zone == 0) {
            memsetVirtPtr(request->buffer, 0, tmp_size);
        } else {
            CHECKED(vfsReadAt(
                request->file->fs->block_device, NULL, request->buffer, tmp_size, offsetForZone(*zone) + block_offset, &tmp_size
            ));
        }
        if (tmp_size == 0) {
            return simpleError(EIO);
        }
        request->buffer.address += tmp_size;
        request->left -= tmp_size;
    }
    if (request->left == 0) {
        return simpleError(SUCCESS_EXIT);
    } else {
        return simpleError(SUCCESS);
    }
}

static Error minixReadWriteFunction(MinixFile* file, Process* process, VirtPtr buffer, size_t length, bool write, size_t* ret) {
    lockTaskLock(&file->lock);
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode),
        offsetForINode(file->fs, file->inodenum), &tmp_size
    ), {
        unlockTaskLock(&file->lock);
    });
    if (inode.size < file->position) {
        *ret = 0;
        unlockTaskLock(&file->lock);
        return simpleError(SUCCESS);
    }
    MinixReadWriteRequest request = {
        .file = file,
        .process = process,
        .buffer = buffer,
        .length = umin(length, inode.size),
        .left = umin(length, inode.size),
        .write = write,
    };
    Error err = minixZoneWalk(file, file->position, &inode, minixGenOpZoneWalkCallback, &request);
    if (err.kind != SUCCESS_EXIT && isError(err)) {
        unlockTaskLock(&file->lock);
        return err;
    } else {
        *ret = request.length;
        file->position += request.length;
        unlockTaskLock(&file->lock);
        return simpleError(SUCCESS);
    }
}

static Error minixReadFunction(MinixFile* file, Process* process, VirtPtr buffer, size_t length, size_t* ret) {
    return minixReadWriteFunction(file, process, buffer, length, false, ret);
}

static Error minixWriteFunction(MinixFile* file, Process* process, VirtPtr buffer, size_t length, size_t* ret) {
    return minixReadWriteFunction(file, process, buffer, length, true, ret);
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
        unlockTaskLock(&request->file->lock);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockTaskLock(&request->file->lock);
        request->callback(simpleError(EIO), 0, request->udata);
        dealloc(request);
    } else {
        size_t new_position = request->inode.size + request->offset; // This is only used for SEEK_END
        request->file->position = new_position;
        unlockTaskLock(&request->file->lock);
        request->callback(simpleError(SUCCESS), new_position, request->udata);
        dealloc(request);
    }
}

static void minixSeekFunction(MinixFile* file, Process* process, size_t offset, VfsSeekWhence whence, VfsFunctionCallbackSizeT callback, void* udata) {
    if (whence == VFS_SEEK_END) { // We have to read the size to know where the end is
        MinixSeekRequest* request = kalloc(sizeof(MinixSeekRequest));
        lockTaskLock(&file->lock);
        request->file = file;
        request->offset = offset;
        request->callback = callback;
        request->udata = udata;
        vfsReadAt(
            file->fs->block_device, NULL, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(file->fs, file->inodenum),
            (VfsFunctionCallbackSizeT)minixSeekINodeCallback, request
        );
    } else {
        lockTaskLock(&file->lock);
        size_t new_position = file->position;
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
        unlockTaskLock(&file->lock);
        callback(simpleError(SUCCESS), new_position, udata);
    }
}

typedef struct {
    MinixFile* file;
    VfsFunctionCallbackStat callback;
    void* udata;
    MinixInode inode;
} MinixStatRequest;

static void minixStatINodeCallback(Error error, size_t read, MinixStatRequest* request) {
    if (isError(error)) {
        unlockTaskLock(&request->file->lock);
        VfsStat ret = {};
        request->callback(error, ret, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockTaskLock(&request->file->lock);
        VfsStat ret = {};
        request->callback(simpleError(EIO), ret, request->udata);
        dealloc(request);
    } else {
        unlockTaskLock(&request->file->lock);
        VfsStat ret = {
            .id = request->file->inodenum,
            .mode = request->inode.mode,
            .nlinks = request->inode.nlinks,
            .uid = request->inode.uid,
            .gid = request->inode.gid,
            .size = request->inode.size,
            .block_size = 1,
            .st_atime = request->inode.atime * 1000000000UL,
            .st_mtime = request->inode.mtime * 1000000000UL,
            .st_ctime = request->inode.ctime * 1000000000UL,
            .dev = request->file->fs->block_device->ino,
        };
        request->callback(simpleError(SUCCESS), ret, request->udata);
    }
}

static void minixStatFunction(
    MinixFile* file, Process* process, VfsFunctionCallbackStat callback, void* udata
) {
    MinixStatRequest* request = kalloc(sizeof(MinixStatRequest));
    lockTaskLock(&file->lock);
    request->file = file;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(&request->inode), sizeof(MinixInode),
        offsetForINode(file->fs, file->inodenum), (VfsFunctionCallbackSizeT)minixStatINodeCallback,
        request
    );
}

static void minixCloseFunction(
    MinixFile* file, Process* process, VfsFunctionCallbackVoid callback, void* udata
) {
    lockTaskLock(&file->lock);
    file->fs->base.open_files--;
    dealloc(file);
    callback(simpleError(SUCCESS), udata);
}

static void minixDupFunction(MinixFile* file, Process* process, VfsFunctionCallbackFile callback, void* udata) {
    lockTaskLock(&file->lock);
    file->fs->base.open_files++;
    MinixFile* copy = kalloc(sizeof(MinixFile));
    memcpy(copy, file, sizeof(MinixFile));
    unlockTaskLock(&file->lock);
    unlockTaskLock(&copy->lock); // Also unlock the new file
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

static void minixTruncFunction(MinixFile* file, Process* process, size_t size, VfsFunctionCallbackVoid callback, void* udata) {
    MinixTruncRequest* request = kalloc(sizeof(MinixStatRequest));
    request->callback = callback;
    request->udata = udata;
    minixGenericOperation(
        file, process, virtPtrForKernel(NULL), size, false, true,
        (VfsFunctionCallbackSizeT)minixTruncCallback, request
    );
}

static Error minixChFunction(
    MinixFile* file, Process* process, VfsMode new_mode, Uid new_uid, Gid new_gid, bool chown,
) {
    lockTaskLock(&file->lock);
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, inodenum), &tmp_size), {
        unlockTaskLock(&file->lock);
    });
    if (tmp_size != sizeof(MinixInode)) {
        unlockTaskLock(&file->lock);
        return simpleError(EIO);
    } else if (process != NULL && process->resources.uid != 0 && process->resources.uid != request->inode.uid) {
        unlockTaskLock(&file->lock);
        return simpleError(EACCES);
    }
    if (chown) {
        inode.uid = new_uid;
        inode.gid = new_gid;
    } else {
        // Don't allow changing the type
        inode.mode &= VFS_MODE_TYPE; // Clear everything but the type
        inode.mode |= new_mode & ~VFS_MODE_TYPE; 
    }
    CHECKED(vfsWriteAt(fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, inodenum), &tmp_size), {
        unlockTaskLock(&file->lock);
    });
    if (tmp_size != sizeof(MinixInode)) {
        unlockTaskLock(&file->lock);
        return simpleError(EIO);
    }
    unlockTaskLock(&file->lock);
    return simpleError(SUCCESS);
}

static Error minixChmodFunction(MinixFile* file, Process* process, VfsMode mode) {
    return minixChFunction(file, process, mode, 0, 0, false);
}

static Error minixChownFunction(MinixFile* file, Process* process, Uid new_uid, Gid new_gid) {
    return minixChFunction(file, process, 0, new_uid, new_gid, true);
}

static Error minixReaddirFunction(MinixFile* file, Process* process, VirtPtr buff, size_t size, size_t* ret) {
    size_t position = file->position;
    size_t tmp_size;
    MinixDirEntry entry;
    CHECKED(file->base.functions->read((VfsFile*)file, process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), &tmp_size));
    if (tmp_size != 0 && tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EACCES);
    } else {
        if (tmp_size == 0) {
            *ret = 0;
            return simpleError(SUCCESS);
        } else {
            size_t name_len = strlen((char*)entry.name);
            size_t vfs_entry_size = sizeof(VfsDirectoryEntry) + name_len + 1;
            VfsDirectoryEntry* vfs_entry = kalloc(vfs_entry_size);
            vfs_entry->id = entry.inode;
            vfs_entry->off = position;
            vfs_entry->len = vfs_entry_size;
            vfs_entry->type = VFS_TYPE_UNKNOWN;
            memcpy(vfs_entry->name, entry.name, name_len + 1);
            memcpyBetweenVirtPtr(request->buff, virtPtrForKernel(vfs_entry), umin(size, vfs_entry_size));
            dealloc(vfs_entry);
            *ret = umin(size, vfs_entry_size);
            return simpleError(SUCCESS);
        }
    }
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

