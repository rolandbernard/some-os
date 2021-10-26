
#include <string.h>

#include "error/error.h"
#include "files/special/pipe.h"

#include "kernel/time.h"
#include "memory/kalloc.h"
#include "util/util.h"

// Shared data should be locked before calling this
static void doOperationOnPipe(PipeSharedData* pipe) {
    for (;;) {
        if (pipe->waiting_reads != NULL && pipe->count != 0) {
            // We can do some reading
            size_t length = umin(pipe->count, pipe->waiting_reads->size);
            if (PIPE_BUFFER_CAPACITY < pipe->read_pos + length) {
                // We need to wrap around
                size_t first = PIPE_BUFFER_CAPACITY - pipe->read_pos;
                memcpyBetweenVirtPtr(pipe->waiting_reads->buffer, virtPtrForKernel(pipe->buffer + pipe->read_pos), first);
                size_t second = length - first;
                pipe->waiting_reads->buffer.address += first;
                memcpyBetweenVirtPtr(pipe->waiting_reads->buffer, virtPtrForKernel(pipe->buffer), second);
                pipe->waiting_reads->buffer.address += second;
            } else {
                memcpyBetweenVirtPtr(pipe->waiting_reads->buffer, virtPtrForKernel(pipe->buffer + pipe->read_pos), length);
                pipe->waiting_reads->buffer.address += length;
            }
            pipe->waiting_reads->size -= length;
            pipe->waiting_reads->written += length;
            pipe->count -= length;
            if (pipe->waiting_reads->size == 0 || pipe->waiting_writes == NULL) {
                WaitingPipeOperation* op = pipe->waiting_reads;
                pipe->waiting_reads = op->next;
                if (pipe->waiting_reads == NULL) {
                    pipe->waiting_reads_tail = NULL;
                }
                op->callback(simpleError(SUCCESS), op->written, op->udata);
                dealloc(op);
            }
        } else if (pipe->waiting_writes != NULL && pipe->count < PIPE_BUFFER_CAPACITY) {
            // We can do some writing
            size_t length = umin(PIPE_BUFFER_CAPACITY - pipe->count, pipe->waiting_writes->size);
            size_t write_pos = (pipe->read_pos + pipe->count) % PIPE_BUFFER_CAPACITY;
            if (PIPE_BUFFER_CAPACITY < write_pos + length) {
                // We need to wrap around
                size_t first = PIPE_BUFFER_CAPACITY - write_pos;
                memcpyBetweenVirtPtr(virtPtrForKernel(pipe->buffer + write_pos), pipe->waiting_writes->buffer, first);
                size_t second = length - first;
                pipe->waiting_writes->buffer.address += first;
                memcpyBetweenVirtPtr(virtPtrForKernel(pipe->buffer), pipe->waiting_writes->buffer, second);
                pipe->waiting_writes->buffer.address += second;
            } else {
                memcpyBetweenVirtPtr(virtPtrForKernel(pipe->buffer + write_pos), pipe->waiting_writes->buffer, length);
                pipe->waiting_writes->buffer.address += length;
            }
            pipe->waiting_writes->size -= length;
            pipe->waiting_writes->written += length;
            pipe->count += length;
            if (pipe->waiting_writes->size == 0) {
                WaitingPipeOperation* op = pipe->waiting_writes;
                pipe->waiting_writes = op->next;
                if (pipe->waiting_writes == NULL) {
                    pipe->waiting_writes_tail = NULL;
                }
                op->callback(simpleError(SUCCESS), op->written, op->udata);
                dealloc(op);
            }
        } else {
            // We can't do anything right now.
            break;
        }
    }
}

static void pipeReadFunction(PipeFile* file, Process* process, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->data->lock);
    WaitingPipeOperation* op = kalloc(sizeof(WaitingPipeOperation));
    op->buffer = buffer;
    op->size = size;
    op->written = 0;
    op->callback = callback;
    op->udata = udata;
    op->next = NULL;
    if (file->data->waiting_reads == NULL) {
        file->data->waiting_reads = op;
        file->data->waiting_reads_tail = op;
    } else {
        file->data->waiting_reads_tail->next = op;
    }
    doOperationOnPipe(file->data);
    unlockSpinLock(&file->data->lock);
}

static void pipeWriteFunction(PipeFile* file, Process* process, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->data->lock);
    WaitingPipeOperation* op = kalloc(sizeof(WaitingPipeOperation));
    op->buffer = buffer;
    op->size = size;
    op->written = 0;
    op->callback = callback;
    op->udata = udata;
    op->next = NULL;
    if (file->data->waiting_writes == NULL) {
        file->data->waiting_writes = op;
        file->data->waiting_writes_tail = op;
    } else {
        file->data->waiting_writes_tail->next = op;
    }
    doOperationOnPipe(file->data);
    unlockSpinLock(&file->data->lock);
}

static void pipeStatFunction(PipeFile* file, Process* process, VfsFunctionCallbackStat callback, void* udata) {
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

static void pipeCloseFunction(PipeFile* file, Process* process, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&file->data->lock);
    file->data->ref_count--;
    if (file->data->ref_count == 0) {
        dealloc(file->data->buffer);
        dealloc(file->data);
    } else {
        unlockSpinLock(&file->data->lock);
    }
    dealloc(file);
    callback(simpleError(SUCCESS), udata);
}

PipeFile* duplicatePipeFile(PipeFile* file) {
    if (file != NULL) {
        lockSpinLock(&file->data->lock);
        file->data->ref_count++;
        unlockSpinLock(&file->data->lock);
        PipeFile* copy = kalloc(sizeof(PipeFile));
        memcpy(copy, file, sizeof(PipeFile));
        return copy;
    } else {
        return NULL;
    }
}

static void pipeDupFunction(PipeFile* file, Process* process, VfsFunctionCallbackFile callback, void* udata) {
    callback(simpleError(SUCCESS), (VfsFile*)duplicatePipeFile(file), udata);
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
    if (file != NULL) {
        file->base.functions = &functions;
        file->base.ino = 0;
        file->data = zalloc(sizeof(PipeSharedData));
        file->data->ref_count = 1;
        file->data->buffer = kalloc(PIPE_BUFFER_CAPACITY);
    }
    return file;
}

