
#include "files/special/fifo.h"

#include "memory/kalloc.h"

typedef struct {
    VfsNode base;
    PipeSharedData* data;
} VfsFifoNode;

VfsFile* createFifoFile(VfsNode* node, char* path) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = node;
    file->path = path;
    file->ref_count = 1;
    file->offset = 0;
    file->flags = 0;
    initTaskLock(&file->lock);
    return file;
}

