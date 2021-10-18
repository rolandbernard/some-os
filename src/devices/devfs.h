#ifndef _DEVFS_H_
#define _DEVFS_H_

#include "files/vfs.h"

#define DEV_INO (size_t)(-1)

// Filesystem mounted as /dev

typedef struct {
    VfsFile base;
    size_t entry;
    SpinLock lock;
} DeviceDirectoryFile;

typedef struct {
    VfsFilesystem base;
} DeviceFilesystem;

DeviceFilesystem* createDeviceFilesystem();

#endif
