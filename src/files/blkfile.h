#ifndef _BLKFILE_H_
#define _BLKFILE_H_

#include "files/vfs.h"
#include "util/spinlock.h"

// File wrapper around a block device operation

typedef void (*BlockOperatonCallback)(Error status, void* udata);

typedef void (*BlockOperationFunction)(
    void* device, VirtPtr buffer, uint32_t offset, uint32_t size, bool write,
    BlockOperatonCallback callback, void* udata
);

typedef struct {
    VfsFile base;
    SpinLock lock;
    size_t position;
    void* device;
    size_t size;
    size_t block_size;
    BlockOperationFunction block_operation;
} BlockDeviceFile;

BlockDeviceFile* createBlockDeviceFile(void* block_dev, size_t block_size, size_t size, BlockOperationFunction block_op);

#endif
