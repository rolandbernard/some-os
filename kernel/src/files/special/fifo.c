
#include <assert.h>
#include <string.h>

#include "files/special/fifo.h"

#include "files/vfs/node.h"
#include "memory/kalloc.h"
#include "util/stringmap.h"

static SpinLock fifo_name_lock;
static StringMap named_data = STRING_MAP_INITIALIZER;

static PipeSharedData* getDataForName(const char* name, const char** unique_name, bool for_write) {
    lockSpinLock(&fifo_name_lock);
    PipeSharedData* ret = getFromStringMap(&named_data, name);
    if (ret == NULL) {
        ret = createPipeSharedData(for_write);
        if (ret != NULL) {
            putToStringMap(&named_data, name, ret);
        }
    } else {
        copyPipeSharedData(ret, for_write);
    }
    *unique_name = getKeyFromStringMap(&named_data, name);
    unlockSpinLock(&fifo_name_lock);
    return ret;
}

static void decreaseReferenceFor(const char* name, bool for_write) {
    lockSpinLock(&fifo_name_lock);
    PipeSharedData* data = getFromStringMap(&named_data, name);
    if (data != NULL) {
        if (freePipeSharedData(data, for_write)) {
            deleteFromStringMap(&named_data, name);
        }
    }
    unlockSpinLock(&fifo_name_lock);
}

typedef struct {
    VfsNode base;
    PipeSharedData* data;
    const char* name;
    bool for_write;
} VfsFifoNode;

static void fifoNodeFree(VfsFifoNode* node) {
    decreaseReferenceFor(node->name, node->for_write);
    vfsNodeClose(node->base.real_node);
    dealloc(node);
}

static Error fifoNodeReadAt(VfsFifoNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read, bool block) {
    return executePipeOperation(node->data, buff, length, false, read, block);
}

static Error fifoNodeWriteAt(VfsFifoNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written, bool block) {
    assert(node->for_write);
    return executePipeOperation(node->data, buff, length, true, written, block);
}

static bool fifoNodeWillBlock(VfsFifoNode* node, bool write) {
    return pipeWillBlock(node->data, write);
}

static const VfsNodeFunctions funcs = {
    .free = (VfsNodeFreeFunction)fifoNodeFree,
    .read_at = (VfsNodeReadAtFunction)fifoNodeReadAt,
    .write_at = (VfsNodeWriteAtFunction)fifoNodeWriteAt,
    .will_block = (VfsNodeWillBlockFunction)fifoNodeWillBlock,
};

VfsFifoNode* createFifoNode(char* path, VfsNode* real_node, bool for_write) {
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
    initTaskLock(&node->base.ref_lock);
    node->base.mounted = NULL;
    node->data = getDataForName(path, &node->name, for_write);
    node->for_write = for_write;
    return node;
}

VfsFile* createFifoFile(VfsNode* node, char* path, bool for_write) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = (VfsNode*)createFifoNode(path, node, for_write);
    file->path = path;
    file->ref_count = 1;
    file->offset = 0;
    file->flags = 0;
    initTaskLock(&file->lock);
    initTaskLock(&file->ref_lock);
    return file;
}

