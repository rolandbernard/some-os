#ifndef _VFS_CACHE_H_
#define _VFS_CACHE_H_

#include "files/vfs/types.h"

// This is a hash map from (superblock id, node id) to the nodes.
// We must use this also for increasing and decreading node reference counts.

VfsNode* vfsCacheGetNodeOrLock(VfsNodeCache* cache, size_t sb_id, size_t node_id);

void vfsCacheUnlock(VfsNodeCache* cache);

void vfsCacheRegister(VfsNodeCache* cache, VfsNode* node);

size_t vfsCacheCopyNode(VfsNodeCache* cache, VfsNode* node);

size_t vfsCacheCloseNode(VfsNodeCache* cache, VfsNode* node);

void vfsCacheInit(VfsNodeCache* cache);

void vfsCacheDeinit(VfsNodeCache* cache);

#endif
