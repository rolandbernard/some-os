#ifndef _VFS_SUPER_H_
#define _VFS_SUPER_H_

#include "files/vfs/types.h"

Error vfsSuperReadNode(VfsSuperblock* sb, size_t id, VfsNode** ret);

Error vfsSuperWriteNode(VfsNode* write);

Error vfsSuperNewNode(VfsSuperblock* sb, VfsNode** ret);

Error vfsSuperFreeNode(VfsNode* node);

void vfsSuperCopy(VfsSuperblock* sb);

void vfsSuperClose(VfsSuperblock* sb);

#endif
