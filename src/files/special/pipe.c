
#include <string.h>

#include "error/error.h"
#include "files/special/pipe.h"

#include "kernel/time.h"
#include "memory/kalloc.h"
#include "util/util.h"

static void pipeReadFunction(PipeFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->data->lock);
    size_t length = umin(size, file->data->count);
    if (file->data->capacity < file->data->read_pos + length) {
        // We need to wrap around
        size_t first = file->data->capacity - file->data->read_pos;
        memcpyBetweenVirtPtr(buffer, virtPtrForKernel(file->data->buffer + file->data->read_pos), first);
        size_t second = length - first;
        buffer.address += first;
        memcpyBetweenVirtPtr(buffer, virtPtrForKernel(file->data->buffer), second);
    } else {
        memcpyBetweenVirtPtr(buffer, virtPtrForKernel(file->data->buffer + file->data->read_pos), length);
    }
    file->data->read_pos = (file->data->read_pos + length) % file->data->capacity;
    file->data->count -= length;
    unlockSpinLock(&file->data->lock);
    callback(simpleError(SUCCESS), length, udata);
}

static void pipeWriteFunction(PipeFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->data->lock);
    if (file->data->count + size > file->data->capacity) {
        // We need more space
        size_t new_capacity = file->data->capacity;
        while (file->data->count + size > new_capacity) {
            new_capacity = new_capacity != 0 ? new_capacity * 3 / 2 : 32;
        }
        char* new_buffer = kalloc(file->data->capacity);
        if (new_buffer == NULL) {
            unlockSpinLock(&file->data->lock);
            callback(simpleError(ALREADY_IN_USE), 0, udata);
            return;
        }
        // Copy the old data
        if (file->data->capacity < file->data->read_pos + file->data->count) {
            // We need to wrap around
            size_t first = file->data->capacity - file->data->read_pos;
            memcpy(new_buffer, file->data->buffer + file->data->read_pos, first);
            size_t second = file->data->count - first;
            memcpy(new_buffer + first, file->data->buffer, second);
        } else {
            memcpy(new_buffer, file->data->buffer + file->data->read_pos, file->data->count);
        }
        file->data->capacity = new_capacity;
        dealloc(file->data->buffer);
        file->data->buffer = new_buffer;
        file->data->read_pos = 0;
    }
    size_t write_pos = (file->data->read_pos + file->data->count) % file->data->capacity;
    if (file->data->capacity < write_pos + size) {
        // We need to wrap around
        size_t first = file->data->capacity - write_pos;
        memcpyBetweenVirtPtr(virtPtrForKernel(file->data->buffer + write_pos), buffer, first);
        size_t second = size - first;
        buffer.address += first;
        memcpyBetweenVirtPtr(virtPtrForKernel(file->data->buffer), buffer, second);
    } else {
        memcpyBetweenVirtPtr(virtPtrForKernel(file->data->buffer + write_pos), buffer, size);
    }
    file->data->count += size;
    unlockSpinLock(&file->data->lock);
    callback(simpleError(SUCCESS), size, udata);
}

static void pipeStatFunction(PipeFile* file, Uid uid, Gid gid, VfsFunctionCallbackStat callback, void* udata) {
    VfsStat ret = {
        .id = file->base.ino,
        .mode = TYPE_MODE(VFS_TYPE_CHAR) | VFS_MODE_OGA_RW,
        .nlinks = 1,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .block_size = 0,
        .st_atime = getNanoseconds(),
        .st_mtime = getNanoseconds(),
        .st_ctime = getNanoseconds(),
        .dev = 0,
    };
    callback(simpleError(SUCCESS), ret, udata);
}

static void pipeCloseFunction(PipeFile* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&file->data->lock);
    file->data->ref_count--;
    if (file->data->count == 0) {
        dealloc(file->data);
    }
    unlockSpinLock(&file->data->lock);
    dealloc(file);
    callback(simpleError(SUCCESS), udata);
}

static void pipeDupFunction(PipeFile* file, Uid uid, Gid gid, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&file->data->lock);
    file->data->ref_count++;
    unlockSpinLock(&file->data->lock);
    PipeFile* copy = kalloc(sizeof(PipeFile));
    memcpy(copy, file, sizeof(PipeFile));
    callback(simpleError(SUCCESS), (VfsFile*)copy, udata);
}

static const VfsFileVtable functions = {
    .read = (ReadFunction)pipeReadFunction,
    .write = (WriteFunction)pipeWriteFunction,
    .stat = (StatFunction)pipeStatFunction,
    .close = (CloseFunction)pipeCloseFunction,
    .dup = (DupFunction)pipeDupFunction,
};

PipeFile* createPipeFile() {
    PipeFile* file = zalloc(sizeof(PipeFile));
    file->base.functions = &functions;
    file->base.ino = 0;
    file->data = zalloc(sizeof(PipeSharedData));
    file->data->count = 1;
    return file;
}

