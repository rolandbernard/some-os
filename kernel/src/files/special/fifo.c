
#include <string.h>

#include "files/special/fifo.h"

#include "files/vfs/node.h"
#include "memory/kalloc.h"
#include "util/stringmap.h"

static SpinLock fifo_name_lock;
static StringMap named_data = STRING_MAP_INITIALIZER;

static PipeSharedData* getDataForName(const char* name, const char** unique_name) {
    lockSpinLock(&fifo_name_lock);
    PipeSharedData* ret = getFromStringMap(&named_data, name);
    if (ret == NULL) {
        ret = createPipeSharedData();
        if (ret != NULL) {
            putToStringMap(&named_data, name, ret);
        }
    } else {
        lockSpinLock(&ret->lock);
        ret->ref_count++;
        unlockSpinLock(&ret->lock);
    }
    *unique_name = getKeyFromStringMap(&named_data, name);
    unlockSpinLock(&fifo_name_lock);
    return ret;
}

static void decreaseReferenceFor(const char* name) {
    lockSpinLock(&fifo_name_lock);
    PipeSharedData* data = getFromStringMap(&named_data, name);
    if (data != NULL) {
        lockSpinLock(&data->lock);
        data->ref_count--;
        if (data->ref_count == 0) {
            unlockSpinLock(&data->lock);
            dealloc(data);
            deleteFromStringMap(&named_data, name);
        } else {
            unlockSpinLock(&data->lock);
        }
    }
    unlockSpinLock(&fifo_name_lock);
}

typedef struct {
    VfsNode base;
    PipeSharedData* data;
    const char* name;
} VfsFifoNode;

static void fifoNodeFree(VfsFifoNode* node) {
    decreaseReferenceFor(node->name);
    vfsNodeClose(node->base.real_node);
    dealloc(node);
}

static Error fifoNodeReadAt(VfsFifoNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    return executePipeOperation(node->data, buff, length, false, read);
}

static Error fifoNodeWriteAt(VfsFifoNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written) {
    return executePipeOperation(node->data, buff, length, true, written);
}

static const VfsNodeFunctions funcs = {
    .free = (VfsNodeFreeFunction)fifoNodeFree,
    .read_at = (VfsNodeReadAtFunction)fifoNodeReadAt,
    .write_at = (VfsNodeWriteAtFunction)fifoNodeWriteAt,
};

VfsFifoNode* createFifoNode(char* path, VfsNode* real_node) {
    VfsFifoNode* node = kalloc(sizeof(VfsFifoNode));
    node->base.functions = &funcs;
    node->base.superblock = NULL;
    memcpy(&node->base.stat, &real_node->stat, sizeof(VfsStat));
    node->base.stat.size = 0;
    node->base.stat.block_size = 0;
    node->base.stat.blocks = 0;
    node->base.real_node = real_node;
    node->base.ref_count = 1;
    initTaskLock(&node->base.lock);
    node->base.mounted = NULL;
    node->data = getDataForName(path, &node->name);
    return node;
}

VfsFile* createFifoFile(VfsNode* node, char* path) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = (VfsNode*)createFifoNode(path, node);
    file->path = path;
    file->ref_count = 1;
    file->offset = 0;
    file->flags = 0;
    initTaskLock(&file->lock);
    return file;
}

