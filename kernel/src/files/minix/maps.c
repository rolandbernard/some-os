
#include "files/minix/maps.h"

#include "memory/kalloc.h"
#include "error/log.h"

typedef struct {
    MinixFilesystem* fs;
    size_t offset;
    size_t size;
    size_t position;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    uint8_t* data;
} MinixGetBitMapRequest;

#define MAX_SINGLE_READ_SIZE (1 << 16)

static void minixFindFreeBitWriteCallback(Error error, size_t written, MinixGetBitMapRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->maps_lock);
        dealloc(request->data);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (written == 0) {
        unlockSpinLock(&request->fs->maps_lock);
        dealloc(request->data);
        request->callback(simpleError(EIO), request->position, request->udata);
        dealloc(request);
    } else {
        unlockSpinLock(&request->fs->maps_lock);
        dealloc(request->data);
        request->callback(simpleError(SUCCESS), request->position, request->udata);
        dealloc(request);
    }
}

static void minixFindFreeBitReadCallback(Error error, size_t read, MinixGetBitMapRequest* request);

static void readBlocksForRequest(MinixGetBitMapRequest* request) {
    size_t size = (request->size + 7) / 8;
    if (size == 0) {
        unlockSpinLock(&request->fs->maps_lock);
        dealloc(request->data);
        request->callback(simpleError(ENOSPC), 0, request->udata);
        dealloc(request);
    } else {
        if (size > MAX_SINGLE_READ_SIZE) {
            size = MAX_SINGLE_READ_SIZE;
        }
        if (request->data == NULL) {
            request->data = kalloc(size);
        }
        vfsReadAt(
            (VfsFile*)request->fs->block_device, NULL,
            virtPtrForKernel(request->data), size, request->offset,
            (VfsFunctionCallbackSizeT)minixFindFreeBitReadCallback, request
        );
    }
}

static void minixFindFreeBitReadCallback(Error error, size_t read, MinixGetBitMapRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->maps_lock);
        dealloc(request->data);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read == 0) {
        unlockSpinLock(&request->fs->maps_lock);
        dealloc(request->data);
        request->callback(simpleError(EIO), 0, request->udata);
        dealloc(request);
    } else {
        for (size_t i = 0; request->size > 0 && i < read; i++) {
            uint8_t byte = request->data[i];
            for (int j = 0; request->size > 0 && j < 8; j++) {
                if (((byte >> j) & 1) == 0) {
                    // This bit is free
                    request->data[i] |= (1 << j);
                    vfsWriteAt(
                        (VfsFile*)request->fs->block_device, NULL,
                        virtPtrForKernel(&request->data[i]), 1, request->offset + i,
                        (VfsFunctionCallbackSizeT)minixFindFreeBitWriteCallback, request
                    );
                    return;
                }
                request->position++;
                request->size--;
            }
        }
        request->offset += read;
        readBlocksForRequest(request);
    }
}

static void genericMinixGetBitMap(MinixFilesystem* fs, size_t offset, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    MinixGetBitMapRequest* request = kalloc(sizeof(MinixGetBitMapRequest));
    lockSpinLock(&fs->maps_lock);
    request->fs = fs;
    request->offset = offset;
    request->size = size;
    request->position = 0;
    request->callback = callback;
    request->udata = udata;
    request->data = NULL;
    readBlocksForRequest(request);
}

void getFreeMinixInode(MinixFilesystem* fs, MinixINodeCallback callback, void* udata) {
    genericMinixGetBitMap(
        fs, 2 * MINIX_BLOCK_SIZE, fs->superblock.ninodes,
        (VfsFunctionCallbackSizeT)callback, udata
    );
}

void getFreeMinixZone(MinixFilesystem* fs, VfsFunctionCallbackSizeT callback, void* udata) {
    genericMinixGetBitMap(
        fs, (2 + fs->superblock.imap_blocks) * MINIX_BLOCK_SIZE, fs->superblock.zones,
        (VfsFunctionCallbackSizeT)callback, udata
    );
}

typedef struct {
    MinixFilesystem* fs;
    size_t offset;
    size_t position;
    char data;
    VfsFunctionCallbackVoid callback;
    void* udata;
} MinixClearBitMapRequest;

static void genericMinixClearBitMapWriteCallback(Error error, size_t written, MinixClearBitMapRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->maps_lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else if (written == 0) {
        unlockSpinLock(&request->fs->maps_lock);
        request->callback(simpleError(EIO), request->udata);
        dealloc(request);
    } else {
        unlockSpinLock(&request->fs->maps_lock);
        request->callback(simpleError(SUCCESS), request->udata);
        dealloc(request);
    }
}

static void genericMinixClearBitMapReadCallback(Error error, size_t read, MinixClearBitMapRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->maps_lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else if (read == 0) {
        unlockSpinLock(&request->fs->maps_lock);
        request->callback(simpleError(EIO), request->udata);
        dealloc(request);
    } else {
        request->data &= ~(1 << (request->position % 8));
        vfsWriteAt(
            (VfsFile*)request->fs->block_device, NULL,
            virtPtrForKernel(&request->data), 1, request->offset + request->position / 8,
            (VfsFunctionCallbackSizeT)genericMinixClearBitMapWriteCallback, request
        );
    }
}

static void genericMinixClearBitMap(MinixFilesystem* fs, size_t offset, size_t position, VfsFunctionCallbackVoid callback, void* udata) {
    MinixClearBitMapRequest* request = kalloc(sizeof(MinixClearBitMapRequest));
    lockSpinLock(&fs->maps_lock);
    request->fs = fs;
    request->offset = offset;
    request->position = position;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        (VfsFile*)request->fs->block_device, NULL,
        virtPtrForKernel(&request->data), 1, offset + position / 8,
        (VfsFunctionCallbackSizeT)genericMinixClearBitMapReadCallback, request
    );
}

void freeMinixInode(MinixFilesystem* fs, uint32_t inode, VfsFunctionCallbackVoid callback, void* udata) {
    genericMinixClearBitMap(fs, 2 * MINIX_BLOCK_SIZE, inode, callback, udata);
}

void freeMinixZone(MinixFilesystem* fs, size_t zone, VfsFunctionCallbackVoid callback, void* udata) {
    genericMinixClearBitMap(
        fs, (2 + fs->superblock.imap_blocks) * MINIX_BLOCK_SIZE, zone, callback, udata
    );
}

