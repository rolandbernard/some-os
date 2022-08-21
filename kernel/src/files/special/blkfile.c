
#include <string.h>

#include "files/special/blkfile.h"

#include "files/vfs/node.h"
#include "memory/kalloc.h"
#include "util/util.h"

typedef struct {
    VfsNode base;
    BlockDevice* device;
} VfsBlockNode;

static void blkNodeFree(VfsBlockNode* node) {
    vfsNodeClose(node->base.real_node);
    dealloc(node);
}

static Error blockOperation(BlockDevice* device, bool write, VirtPtr buffer, size_t offset, size_t size) {
    if (write) {
        return device->functions->write(device, buffer, offset, size);
    } else {
        return device->functions->read(device, buffer, offset, size);
    }
}

static Error genericBlockFileFunction(VfsBlockNode* node, bool write, VirtPtr buffer, size_t offset, size_t size, size_t* ret) {
    size_t block_size = node->device->block_size;
    char* tmp_buffer = kalloc(block_size);
    size_t left = size;
    while (left > 0) {
        size_t size_diff;
        if (offset % block_size == 0 && left > block_size) {
            size_diff = left & -block_size;
            CHECKED(blockOperation(node->device, write, buffer, offset, size_diff), dealloc(tmp_buffer));
        } else {
            size_diff = umin(left, block_size - offset % block_size);
            size_t tmp_start = offset & -block_size;
            CHECKED(blockOperation(node->device, false, virtPtrForKernel(tmp_buffer), tmp_start, block_size), dealloc(tmp_buffer));
            if (write) {
                memcpyBetweenVirtPtr(virtPtrForKernel(tmp_buffer + offset % block_size), buffer, size_diff);
                CHECKED(blockOperation(node->device, true, virtPtrForKernel(tmp_buffer), tmp_start, block_size), dealloc(tmp_buffer));
            } else {
                memcpyBetweenVirtPtr(buffer, virtPtrForKernel(tmp_buffer + offset % block_size), size_diff);
            }
        }
        left -= size_diff;
        offset += size_diff;
        buffer.address += size_diff;
    }
    dealloc(tmp_buffer);
    *ret = size - left;
    return simpleError(SUCCESS);
}

static Error blkNodeReadAt(VfsBlockNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    return genericBlockFileFunction(node, false, buff, offset, length, read);
}

static Error blkNodeWriteAt(VfsBlockNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written) {
    return genericBlockFileFunction(node, true, buff, offset, length, written);
}

VfsNodeFunctions funcs = {
    .free = (VfsNodeFreeFunction)blkNodeFree,
    .read_at = (VfsNodeReadAtFunction)blkNodeReadAt,
    .write_at = (VfsNodeWriteAtFunction)blkNodeWriteAt,
};

VfsBlockNode* createBlkNode(BlockDevice* device, VfsNode* real_node) {
    VfsBlockNode* node = kalloc(sizeof(VfsBlockNode));
    node->base.functions = &funcs;
    node->base.superblock = NULL;
    memcpy(&node->base.stat, &real_node->stat, sizeof(VfsStat));
    node->base.stat.rdev = device->base.id;
    node->base.stat.size = device->size;
    node->base.stat.block_size = device->block_size;
    node->base.stat.blocks = device->size / device->block_size;
    node->base.real_node = real_node;
    node->base.ref_count = 1;
    initTaskLock(&node->base.lock);
    node->base.mounted = NULL;
    node->device = device;
    return node;
}

VfsFile* createBlockDeviceFile(VfsNode* node, BlockDevice* device, char* path, size_t offset) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = (VfsNode*)createBlkNode(device, node);
    file->path = path;
    file->ref_count = 1;
    file->offset = offset;
    file->flags = 0;
    initTaskLock(&file->lock);
    return file;
}

