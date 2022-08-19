
#include "files/vfs/super.h"

#include "files/vfs/cache.h"
#include "files/vfs/node.h"

Error vfsSuperReadNode(VfsSuperblock* sb, size_t id, VfsNode** ret) {
}

Error vfsSuperWriteNode(VfsNode* write) {
}

Error vfsSuperNewNode(VfsSuperblock* sb, VfsNode** ret) {
}

Error vfsSuperFreeNode(VfsNode* node) {
    CHECKED(vfsNodeTrunc(node, NULL, 0));
    return node->superblock->functions->free_node(node->superblock, node);
}

Error vfsSuperCopyNode(VfsNode* node) {
    vfsCacheCopyNode(&node->superblock->nodes, node);
    return simpleError(SUCCESS);
}

Error vfsSuperCloseNode(VfsNode* node) {
    vfsCacheCloseNode(&node->superblock->nodes, node);
    if (node->ref_count == 0) {
        // If ref_count is 0 it was removed from the cache. We can free it.
        if (node->stat.nlinks == 0) {
            // There are no other links to this node, we should free the data.
            CHECKED(vfsSuperFreeNode(node));
        }
        node->functions->free(node);
    }
    return simpleError(SUCCESS);
}

void vfsSuperCopy(VfsSuperblock* sb) {
    lockTaskLock(&sb->lock);
    sb->ref_count++;
    unlockTaskLock(&sb->lock);
}

void vfsSuperClose(VfsSuperblock* sb) {
    lockTaskLock(&sb->lock);
    sb->ref_count--;
    unlockTaskLock(&sb->lock);
}

