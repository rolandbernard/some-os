
#include <assert.h>
#include <string.h>

#include "files/special/pipe.h"

#include "error/error.h"
#include "files/vfs/node.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "task/schedule.h"
#include "task/syscall.h"
#include "util/util.h"

// Shared data should be locked before calling this
static void doOperationOnPipe(PipeSharedData* pipe) {
    for (;;) {
        if (pipe->waiting_reads != NULL && (pipe->count != 0 || pipe->write_count == 0)) {
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
            pipe->read_pos = (pipe->read_pos + length) % PIPE_BUFFER_CAPACITY;
            pipe->count -= length;
            if (pipe->waiting_reads->size == 0 || pipe->waiting_writes == NULL) {
                WaitingPipeOperation* op = pipe->waiting_reads;
                pipe->waiting_reads = op->next;
                if (pipe->waiting_reads == NULL) {
                    pipe->waiting_reads_tail = NULL;
                }
                if (op->wakeup != NULL) {
                    moveTaskToState(op->wakeup, ENQUABLE);
                    enqueueTask(op->wakeup);
                }
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
                if (op->wakeup != NULL) {
                    moveTaskToState(op->wakeup, ENQUABLE);
                    enqueueTask(op->wakeup);
                }
            }
        } else {
            // We can't do anything right now.
            break;
        }
    }
}

static void waitForPipeOperation(void* _, Task* task, PipeSharedData* data) {
    moveTaskToState(task, WAITING);
    enqueueTask(task);
    doOperationOnPipe(data);
    unlockSpinLock(&data->lock);
    runNextTask();
}

static size_t currentReadCapacity(PipeSharedData* data) {
    size_t capacity = data->count;
    WaitingPipeOperation* current = data->waiting_writes;
    while (current != NULL) {
        capacity += current->size;
        current = current->next;
    }
    return capacity;
}

static size_t currentWriteCapacity(PipeSharedData* data) {
    size_t capacity = PIPE_BUFFER_CAPACITY - data->count;
    WaitingPipeOperation* current = data->waiting_reads;
    while (current != NULL) {
        capacity += current->size;
        current = current->next;
    }
    return capacity;
}

Error executePipeOperation(PipeSharedData* data, VirtPtr buffer, size_t size, bool write, size_t* ret, bool block) {
    Task* task = criticalEnter();
    assert(task != NULL);
    lockSpinLock(&data->lock);
    if (currentReadCapacity(data) == 0 && data->write_count == 0) {
        // EOF if there are no open file descriptors for writing
        unlockSpinLock(&data->lock);
        criticalReturn(task);
        *ret = 0;
        return simpleError(SUCCESS);
    } else if (block || (write && currentWriteCapacity(data) >= size) || (!write && currentReadCapacity(data) > 0)) {
        WaitingPipeOperation op;
        op.buffer = buffer;
        op.size = size;
        op.written = 0;
        op.next = NULL;
        op.wakeup = task;
        if (write) {
            if (data->waiting_writes == NULL) {
                data->waiting_writes = &op;
                data->waiting_writes_tail = &op;
            } else {
                data->waiting_writes_tail->next = &op;
            }
        } else {
            if (data->waiting_reads == NULL) {
                data->waiting_reads = &op;
                data->waiting_reads_tail = &op;
            } else {
                data->waiting_reads_tail->next = &op;
            }
        }
        if (saveToFrame(&task->frame)) {
            callInHart((void*)waitForPipeOperation, task, data);
        }
        *ret = op.written;
        return simpleError(SUCCESS);
    } else {
        unlockSpinLock(&data->lock);
        criticalReturn(task);
        return simpleError(EAGAIN);
    }
}

PipeSharedData* createPipeSharedData(bool for_write) {
    PipeSharedData* data = zalloc(sizeof(PipeSharedData));
    data->ref_count = 1;
    data->write_count = for_write ? 1 : 0;
    data->buffer = kalloc(PIPE_BUFFER_CAPACITY);
    return data;
}

void copyPipeSharedData(PipeSharedData* data, bool for_write) {
    lockSpinLock(&data->lock);
    data->ref_count++;
    if (for_write) {
        data->write_count++;
    }
    unlockSpinLock(&data->lock);
}

bool freePipeSharedData(PipeSharedData* data, bool for_write) {
    lockSpinLock(&data->lock);
    data->ref_count--;
    if (for_write) {
        assert(data->write_count > 0);
        data->write_count--;
        if (data->write_count == 0) {
            // EOF all of the remaining reads
            doOperationOnPipe(data);
        }
    }
    assert(data->write_count <= data->ref_count);
    if (data->ref_count == 0) {
        unlockSpinLock(&data->lock);
        dealloc(data->buffer);
        dealloc(data);
        return true;
    } else {
        unlockSpinLock(&data->lock);
        return false;
    }
}

typedef struct {
    VfsNode base;
    PipeSharedData* data;
    bool for_write;
} VfsPipeNode;

static void pipeNodeFree(VfsPipeNode* node) {
    freePipeSharedData(node->data, node->for_write);
    dealloc(node);
}

static Error pipeNodeReadAt(VfsPipeNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read, bool block) {
    return executePipeOperation(node->data, buff, length, false, read, block);
}

static Error pipeNodeWriteAt(VfsPipeNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written, bool block) {
    assert(node->for_write);
    return executePipeOperation(node->data, buff, length, true, written, block);
}

bool pipeWillBlock(PipeSharedData* data, bool write) {
    bool result;
    lockSpinLock(&data->lock);
    if (write) {
        result = currentWriteCapacity(data) == 0;
    } else {
        result = currentReadCapacity(data) == 0 && data->write_count > 0;
    }
    unlockSpinLock(&data->lock);
    return result;
}

static bool pipeNodeWillBlock(VfsPipeNode* node, bool write) {
    return pipeWillBlock(node->data, write);
}

static const VfsNodeFunctions funcs = {
    .free = (VfsNodeFreeFunction)pipeNodeFree,
    .read_at = (VfsNodeReadAtFunction)pipeNodeReadAt,
    .write_at = (VfsNodeWriteAtFunction)pipeNodeWriteAt,
    .will_block = (VfsNodeWillBlockFunction)pipeNodeWillBlock,
};

VfsPipeNode* createPipeNode(PipeSharedData* data, bool for_write) {
    VfsPipeNode* node = kalloc(sizeof(VfsPipeNode));
    node->base.functions = &funcs;
    node->base.superblock = NULL;
    memset(&node->base.stat, 0, sizeof(VfsStat));
    // Everything is allowed.
    node->base.stat.mode = TYPE_MODE(VFS_TYPE_UNKNOWN) | 0777;
    Time time = getNanosecondsWithFallback();
    node->base.stat.atime = time;
    node->base.stat.mtime = time;
    node->base.stat.ctime = time;
    node->base.real_node = (VfsNode*)node;
    node->base.ref_count = 1;
    initTaskLock(&node->base.lock);
    node->base.mounted = NULL;
    node->data = data;
    node->for_write = for_write;
    return node;
}

static VfsFile* createPipeFileWithData(PipeSharedData* data, bool for_write) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = (VfsNode*)createPipeNode(data, for_write);
    file->path = NULL;
    file->ref_count = 1;
    file->offset = 0;
    file->flags = 0;
    initTaskLock(&file->lock);
    return file;
}

VfsFile* createPipeFile(bool for_write) {
    return createPipeFileWithData(createPipeSharedData(for_write), for_write);
}

VfsFile* createPipeFileClone(VfsFile* file, bool for_write) {
    PipeSharedData* data = ((VfsPipeNode*)file->node)->data;
    copyPipeSharedData(data, for_write);
    return createPipeFileWithData(data, for_write);
}

