#ifndef _CHRFILE_H_
#define _CHRFILE_H_

#include "files/vfs/types.h"
#include "task/spinlock.h"

// File wrapper around a character device

VfsFile* createTtyDeviceFile(VfsNode* node, TtyDevice* device, char* path);

#endif
