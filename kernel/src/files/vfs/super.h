#ifndef _VFS_SUPER_H_
#define _VFS_SUPER_H_

#include "files/vfs/types.h"

void vfsSuperCopy(VfsSuperblock* file);

void vfsSuperClose(VfsSuperblock* file);

#endif
