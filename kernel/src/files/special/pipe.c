
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

Error executePipeOperation(PipeSharedData* data, VirtPtr buffer, size_t size, bool write, size_t* ret) {
    WaitingPipeOperation op;
    op.buffer = buffer;
    op.size = size;
    op.written = 0;
    op.next = NULL;
    Task* task = criticalEnter();
    assert(task != NULL);
    op.wakeup = task;
    lockSpinLock(&data->lock);
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
}

PipeSharedData* createPipeSharedData() {
    PipeSharedData* data = zalloc(sizeof(PipeSharedData));
    data->ref_count = 1;
    data->buffer = kalloc(PIPE_BUFFER_CAPACITY);
    return data;
}

void copyPipeSharedData(PipeSharedData* data) {
    lockSpinLock(&data->lock);
    data->ref_count++;
    unlockSpinLock(&data->lock);
}

void freePipeSharedData(PipeSharedData* data) {
    lockSpinLock(&data->lock);
    data->ref_count--;
    if (data->ref_count == 0) {
        unlockSpinLock(&data->lock);
        dealloc(data->buffer);
        dealloc(data);
    } else {
        unlockSpinLock(&data->lock);
    }
}

typedef struct {
    VfsNode base;
    PipeSharedData* data;
} VfsPipeNode;

static void pipeNodeFree(VfsPipeNode* node) {
    freePipeSharedData(node->data);
    dealloc(node);
}

static Error pipeNodeReadAt(VfsPipeNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    return executePipeOperation(node->data, buff, length, false, read);
}

static Error pipeNodeWriteAt(VfsPipeNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written) {
    return executePipeOperation(node->data, buff, length, true, written);
}

static const VfsNodeFunctions funcs = {
    .free = (VfsNodeFreeFunction)pipeNodeFree,
    .read_at = (VfsNodeReadAtFunction)pipeNodeReadAt,
    .write_at = (VfsNodeWriteAtFunction)pipeNodeWriteAt,
};

VfsPipeNode* createPipeNode() {
    VfsPipeNode* node = kalloc(sizeof(VfsPipeNode));
    node->base.functions = &funcs;
    node->base.superblock = NULL;
    memset(&node->base.stat, 0, sizeof(VfsStat));
    // Everything is allowed.
    node->base.stat.mode = TYPE_MODE(VFS_TYPE_UNKNOWN) | 0777;
    Time time = getNanoseconds();
    node->base.stat.atime = time;
    node->base.stat.mtime = time;
    node->base.stat.ctime = time;
    node->base.real_node = (VfsNode*)node;
    node->base.ref_count = 0;
    initTaskLock(&node->base.lock);
    node->base.mounted = NULL;
    node->data = createPipeSharedData();
    return node;
}

static VfsFile* createPipeFileWithNode(VfsNode* node) {
    vfsNodeCopy(node);
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = node;
    file->path = NULL;
    file->ref_count = 1;
    file->offset = 0;
    file->flags = 0;
    initTaskLock(&file->lock);
    return file;
}

VfsFile* createPipeFile() {
    return createPipeFileWithNode((VfsNode*)createPipeNode());
}

VfsFile* createPipeFileClone(VfsFile* file) {
    return createPipeFileWithNode(file->node);
}

