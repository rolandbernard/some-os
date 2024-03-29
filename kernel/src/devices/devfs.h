#ifndef _DEVFS_H_
#define _DEVFS_H_

#include "files/vfs/types.h"

// Filesystem mounted as /dev

typedef struct {
    VfsSuperblock base;
} DeviceFilesystem;

Error createDeviceSuperblock(VfsFile* file, VirtPtr data, VfsSuperblock** out);

Error registerFsDriverDevfs();

#endif
