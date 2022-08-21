#ifndef _DEVFS_H_
#define _DEVFS_H_

#include "files/vfs/types.h"

#define DEV_INO (size_t)(-1)

// Filesystem mounted as /dev

typedef struct {
    VfsNode base;
} DeviceDirectoryNode;

typedef struct {
    VfsSuperblock base;
} DeviceFilesystem;

DeviceFilesystem* createDeviceFilesystem();

#endif
