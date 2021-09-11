#ifndef _BLKFILE_H_
#define _BLKFILE_H_

#include "files/vfs.h"

// File wrapper around a block device operation

typedef void (*BlockOperatonCallback)(Error status, void* udata);

typedef Error (*BlockOperationFunction)(
    void* device, VirtPtr buffer, uint32_t offset, uint32_t size, bool write,
    BlockOperatonCallback callback, void* udata
);

typedef struct {
    VfsFile file;
    size_t position;
    void* device;
    BlockOperationFunction block_operation;
} BlockDeviceFile;

BlockDeviceFile* createBlockDeviceFile(BlockOperationFunction block_op, void* block_dev);

#endif
