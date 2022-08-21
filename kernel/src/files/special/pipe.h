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
    SpinLock lock;
    char* buffer;
    size_t read_pos;
    size_t count;
    WaitingPipeOperation* waiting_reads;
    WaitingPipeOperation* waiting_reads_tail;
    WaitingPipeOperation* waiting_writes;
    WaitingPipeOperation* waiting_writes_tail;
} PipeSharedData;

PipeSharedData* createPipeSharedData();

void freePipeSharedData(PipeSharedData* data);

Error executePipeOperation(PipeSharedData* data, Process* process, VirtPtr buffer, size_t size, bool write, size_t* ret);

VfsFile* createPipeFile();

#endif
