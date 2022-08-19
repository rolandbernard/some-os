#ifndef _VFS_CACHE_H_
#define _VFS_CACHE_H_

#include "files/vfs/types.h"

// This is a hash map from (superblock id, node id) to the nodes.
// We must use this also for increasing and decreading node reference counts.

VfsNode* vfsCacheGetNodeOrLock(VfsNodeCache* cache, size_t sb_id, size_t node_id);

void vfsCacheRegisterNodeAndUnlock(VfsNodeCache* cache, VfsNode* node);

void vfsCacheCopyNode(VfsNodeCache* cache, VfsNode* node);

void vfsCacheCloseNode(VfsNodeCache* cache, VfsNode* node);

#endif
