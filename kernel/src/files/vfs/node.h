#ifndef _VFS_NODE_H_
#define _VFS_NODE_H_

#include "files/vfs/types.h"

void vfsNodeCopy(VfsNode* file);

void vfsNodeClose(VfsNode* file);

#endif
