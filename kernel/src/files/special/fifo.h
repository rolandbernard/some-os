#ifndef _FIFO_H_
#define _FIFO_H_

// For now we are reusing the pipe implementation 

#include "files/special/pipe.h"

typedef struct {
    VfsFile base;
    const char* name;
    Uid uid;
    Gid gid;
    PipeSharedData* data;
} FifoFile;

FifoFile* createFifoFile(const char* path, VfsMode mode, Uid uid, Gid gid);

#endif
