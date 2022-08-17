#ifndef _BLKFILE_H_
#define _BLKFILE_H_

#include "files/vfs/types.h"
#include "task/spinlock.h"

// File wrapper around a block device

VfsFile* createBlockDeviceFile(VfsNode* node, BlockDevice* device, char* path, size_t offset);

#endif
