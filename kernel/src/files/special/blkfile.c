
#include <string.h>

#include "files/special/blkfile.h"

#include "error/error.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "error/log.h"
#include "util/util.h"
#include "devices/devfs.h"

static void blockSeekFunction(BlockDeviceFile* file, Process* process, size_t offset, VfsSeekWhence whence, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->lock);
    size_t new_position = 0;
    switch (whence) {
        case VFS_SEEK_CUR:
            new_position = file->position + offset;
            break;
        case VFS_SEEK_SET:
            new_position = offset;
            break;
        case VFS_SEEK_END:
            new_position = file->size + offset;
            break;
    }
    file->position = new_position;
    unlockSpinLock(&file->lock);
    callback(simpleError(SUCCESS), new_position, udata);
}

typedef struct {
    void* block_dev;
    size_t block_size;
    bool write;
    bool tmp_read;
    BlockOperationFunction block_op;
    VirtPtr buffer;
    size_t offset;
    size_t size;
    size_t read;
    size_t current_read;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    uint8_t tmp_buffer[];
} BlockFileRequest;

static void blockOperatonFileCallback(Error status, BlockFileRequest* request) {
    if (isError(status)) {
        request->callback(status, request->read, request->udata);
        dealloc(request);
    } else {
        if (request->current_read != 0) {
            if (!request->write) {
                size_t offset = request->offset % request->block_size;
                if (offset != 0 || request->current_read < request->block_size) {
                    memcpyBetweenVirtPtr(request->buffer, virtPtrForKernel(request->tmp_buffer + offset), request->current_read);
                } // Otherwise we have already read directly to the buffer
            }
            request->read += request->current_read;
            request->offset += request->current_read;
            request->size -= request->current_read;
            request->buffer.address += request->current_read;
        }
        if (request->size == 0) {
            request->callback(simpleError(SUCCESS), request->read, request->udata);
            dealloc(request);
        } else {
            if (request->offset % request->block_size == 0) {
                if (request->size < request->block_size) {
                    request->current_read = request->size;
                    if (request->write && !request->tmp_read) {
                        request->tmp_read = true;
                        request->current_read = 0;
                        // We first have to load the current data into the tmp buffer
                        request->block_op(
                            request->block_dev, virtPtrForKernel(request->tmp_buffer), request->offset,
                            request->block_size, false,
                            (BlockOperatonCallback)blockOperatonFileCallback, request
                        );
                    } else {
                        request->tmp_read = false;
                        request->current_read = request->size;
                        if (request->write) {
                            memcpyBetweenVirtPtr(virtPtrForKernel(request->tmp_buffer), request->buffer, request->current_read);
                        }
                        request->block_op(
                            request->block_dev, virtPtrForKernel(request->tmp_buffer),
                            request->offset, request->block_size, request->write,
                            (BlockOperatonCallback)blockOperatonFileCallback, request
                        );
                    }
                } else {
                    request->tmp_read = false;
                    size_t read_end = (request->offset + request->size) & -request->block_size;
                    request->current_read = read_end - request->offset; // This is a multiple of the block size
                    request->block_op(
                        request->block_dev, request->buffer, request->offset, request->current_read,
                        request->write, (BlockOperatonCallback)blockOperatonFileCallback, request
                    );
                }
            } else {
                size_t read_start = request->offset & -request->block_size;
                if (request->write && !request->tmp_read) {
                    request->tmp_read = true;
                    request->current_read = 0;
                    // We first have to load the current data into the tmp buffer
                    request->block_op(
                        request->block_dev, virtPtrForKernel(request->tmp_buffer), read_start,
                        request->block_size, false,
                        (BlockOperatonCallback)blockOperatonFileCallback, request
                    );
                } else {
                    request->tmp_read = false;
                    request->current_read = umin(request->size, read_start + request->block_size - request->offset);
                    if (request->write) {
                        size_t offset = request->offset % request->block_size;
                        memcpyBetweenVirtPtr(virtPtrForKernel(request->tmp_buffer + offset), request->buffer, request->current_read);
                    }
                    request->block_op(
                        request->block_dev, virtPtrForKernel(request->tmp_buffer), read_start,
                        request->block_size, request->write,
                        (BlockOperatonCallback)blockOperatonFileCallback, request
                    );
                }
            }
        }
    }
}

static void genericBlockFileFunction(BlockDeviceFile* file, bool write, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    BlockFileRequest request;
    lockSpinLock(&file->lock);
    if (file->position > file->size) {
        size = 0;
    } else if (file->position + size > file->size) {
        size = file->size - file->position;
    }
    request.block_dev = file->device;
    request.block_size = file->block_size;
    request.block_op = file->block_operation;
    request.buffer = buffer;
    request.offset = file->position;
    request.size = size;
    file->position += size;
    unlockSpinLock(&file->lock);
    BlockFileRequest* req = kalloc(sizeof(BlockFileRequest) + request.block_size);
    memcpy(req, &request, sizeof(BlockFileRequest));
    req->write = write;
    req->read = 0;
    req->current_read = 0;
    req->tmp_read = false;
    req->callback = callback;
    req->udata = udata;
    blockOperatonFileCallback(simpleError(SUCCESS), req);
}

static void blockReadFunction(BlockDeviceFile* file, Process* process, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    genericBlockFileFunction(file, false, buffer, size, callback, udata);
}

static void blockWriteFunction(BlockDeviceFile* file, Process* process, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    genericBlockFileFunction(file, true, buffer, size, callback, udata);
}

static void blockStatFunction(BlockDeviceFile* file, Process* process, VfsFunctionCallbackStat callback, void* udata) {
    lockSpinLock(&file->lock);
    VfsStat ret = {
        .id = file->base.ino,
        .mode = TYPE_MODE(VFS_TYPE_BLOCK) | VFS_MODE_OG_RW,
        .nlinks = 1,
        .uid = 0,
        .gid = 0,
        .size = file->block_size,
        .block_size = file->block_size,
        .st_atime = getNanoseconds(),
        .st_mtime = getNanoseconds(),
        .st_ctime = getNanoseconds(),
        .dev = DEV_INO,
    };
    unlockSpinLock(&file->lock);
    callback(simpleError(SUCCESS), ret, udata);
}

static void blockCloseFunction(BlockDeviceFile* file, Process* process, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&file->lock);
    dealloc(file);
    callback(simpleError(SUCCESS), udata);
}

static void blockDupFunction(BlockDeviceFile* file, Process* process, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&file->lock);
    BlockDeviceFile* copy = kalloc(sizeof(BlockDeviceFile));
    memcpy(copy, file, sizeof(BlockDeviceFile));
    unlockSpinLock(&file->lock);
    unlockSpinLock(&copy->lock); // Also unlock the new file
    callback(simpleError(SUCCESS), (VfsFile*)copy, udata);
}

static const VfsFileVtable functions = {
    .seek = (SeekFunction)blockSeekFunction,
    .read = (ReadFunction)blockReadFunction,
    .write = (WriteFunction)blockWriteFunction,
    .stat = (StatFunction)blockStatFunction,
    .close = (CloseFunction)blockCloseFunction,
    .dup = (DupFunction)blockDupFunction,
};

BlockDeviceFile* createBlockDeviceFile(size_t ino, void* block_dev, size_t block_size, size_t size, BlockOperationFunction block_op) {
    BlockDeviceFile* file = zalloc(sizeof(BlockDeviceFile));
    if (file != NULL) {
        file->base.functions = &functions;
        file->base.ino = ino;
        file->device = block_dev;
        file->size = size;
        file->block_size = block_size;
        file->block_operation = block_op;
    }
    return file;
}

