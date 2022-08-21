
#include "files/special/ttyfile.h"

#include "memory/kalloc.h"

typedef struct {
    VfsNode base;
    TtyDevice* device;
} VfsTtyNode;

VfsTtyNode* createTtyNode(TtyDevice* device, VfsNode* real_node) {
    // TODO
    return NULL;
}

VfsFile* createTtyDeviceFile(VfsNode* node, TtyDevice* device, char* path) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = (VfsNode*)createTtyNode(device, node);
    file->path = path;
    file->ref_count = 1;
    file->offset = 0;
    initTaskLock(&file->lock);
    return file;
}

