#ifndef _PIPE_H_
#define _PIPE_H_

#include <stddef.h>

#include "files/vfs.h"
#include "util/spinlock.h"
#include "process/types.h"

#define PIPE_BUFFER_CAPACITY 512

typedef struct WaitingPipeOperation_s {
    VirtPtr buffer;
    size_t size;
    size_t written;
    VfsFunctionCallbackSizeT callback;
    void* udata;
    struct WaitingPipeOperation_s* next;
} WaitingPipeOperation;

typedef struct {
    size_t ref_count;
    SpinLock lock;
    char* buffer;
    size_t read_pos;
    size_t count;
    WaitingPipeOperation* waiting_reads;
    WaitingPipeOperation* waiting_reads_tail;
    WaitingPipeOperation* waiting_writes;
    WaitingPipeOperation* waiting_writes_tail;
} PipeSharedData;

typedef struct {
    VfsFile base;
    PipeSharedData* data;
} PipeFile;

PipeFile* createPipeFile();

PipeFile* duplicatePipeFile(PipeFile* file);

#endif
