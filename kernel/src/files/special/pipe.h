#ifndef _PIPE_H_
#define _PIPE_H_

#include <stddef.h>

#include "task/spinlock.h"
#include "process/types.h"

#define PIPE_BUFFER_CAPACITY 512

typedef struct WaitingPipeOperation_s {
    VirtPtr buffer;
    size_t size;
    size_t written;
    Task* wakeup;
    struct WaitingPipeOperation_s* next;
} WaitingPipeOperation;

typedef struct PipeSharedData_s {
    size_t ref_count;
    size_t write_count;
    SpinLock lock;
    char* buffer;
    size_t read_pos;
    size_t count;
    WaitingPipeOperation* waiting_reads;
    WaitingPipeOperation* waiting_reads_tail;
    WaitingPipeOperation* waiting_writes;
    WaitingPipeOperation* waiting_writes_tail;
} PipeSharedData;

PipeSharedData* createPipeSharedData(bool for_write);

void copyPipeSharedData(PipeSharedData* data, bool for_write);

bool freePipeSharedData(PipeSharedData* data, bool for_write);

Error executePipeOperation(PipeSharedData* data, VirtPtr buffer, size_t size, bool write, size_t* ret, bool block);

bool pipeIsReady(PipeSharedData* data, bool write);

VfsFile* createPipeFile(bool for_write);

VfsFile* createPipeFileClone(VfsFile* file, bool for_write);

#endif
