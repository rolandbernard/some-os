
#include "files/special/blkfile.h"

#include "memory/kalloc.h"

typedef struct {
    VfsNode base;
    BlockDevice* device;
} VfsBlockNode;

VfsFile* createBlockDeviceFile(VfsNode* node, BlockDevice* device, char* path, size_t offset) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = node;
    file->path = path;
    file->ref_count = 1;
    file->offset = offset;
    file->flags = 0;
    initTaskLock(&file->lock);
    return file;
}

