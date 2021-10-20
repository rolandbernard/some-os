#ifndef _PIPE_H_
#define _PIPE_H_

#include <stddef.h>

#include "files/vfs.h"
#include "util/spinlock.h"

typedef struct {
    size_t ref_count;
    SpinLock lock;
    char* buffer;
    size_t capacity;
    size_t read_pos;
    size_t count;
} PipeSharedData;

typedef struct {
    VfsFile base;
    PipeSharedData* data;
} PipeFile;

PipeFile* createPipeFile();

#endif
