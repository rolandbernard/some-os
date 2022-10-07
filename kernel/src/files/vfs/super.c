
#include <assert.h>

#include "files/vfs/super.h"

#include "files/vfs/cache.h"
#include "files/vfs/node.h"
#include "kernel/time.h"

Error vfsSuperReadNode(VfsSuperblock* sb, size_t id, VfsNode** ret) {
    if (id == sb->root_node->stat.id) {
        vfsNodeCopy(sb->root_node);
        *ret = sb->root_node;
        return simpleError(SUCCESS);
    } else {
        VfsNode* node = vfsCacheGetNodeOrLock(&sb->nodes, sb->id, id);
        if (node != NULL) {
            *ret = node;
            return simpleError(SUCCESS);
        } else {
            Error err = sb->functions->read_node(sb, id, &node);
            if (!isError(err)) {
                assert(node->ref_count == 0);
                vfsNodeCopy(node);
                *ret = node;
            }
            vfsCacheUnlock(&sb->nodes);
            return err;
        }
    }
}

Error vfsSuperWriteNode(VfsNode* write) {
    if (write->superblock == NULL) {
        // This is not a real node.
        return simpleError(SUCCESS);
    } else {
        lockTaskLock(&write->lock);
        write->stat.ctime = getNanosecondsWithFallback();
        // The node is marked dirty and written only when closing the node.
        write->dirty = true;
        unlockTaskLock(&write->lock);
        return simpleError(SUCCESS);
    }
}

Error vfsSuperNewNode(VfsSuperblock* sb, VfsNode** ret) {
    size_t new_id;
    CHECKED(sb->functions->new_node(sb, &new_id));
    return vfsSuperReadNode(sb, new_id, ret);
}

Error vfsSuperFreeNode(VfsNode* node) {
    assert(node->superblock != NULL);
    CHECKED(vfsNodeTrunc(node, NULL, 0));
    return node->superblock->functions->free_node(node->superblock, node);
}

Error vfsSuperCopyNode(VfsNode* node) {
    if (node->superblock == NULL) {
        lockTaskLock(&node->ref_lock);
        node->ref_count++;
        unlockTaskLock(&node->ref_lock);
        return simpleError(SUCCESS);
    } else {
        size_t refs = vfsCacheCopyNode(&node->superblock->nodes, node);
        if (refs == 1) {
            vfsSuperCopy(node->superblock);
        }
        return simpleError(SUCCESS);
    }
}

Error vfsSuperCloseNode(VfsNode* node) {
    if (node->superblock == NULL) {
        // This is a special file (e.g. pipe, fifo, char, block)
        // These are different, because they are not cached.
        lockTaskLock(&node->ref_lock);
        node->ref_count--;
        if (node->ref_count == 0) {
            unlockTaskLock(&node->ref_lock);
            node->functions->free(node);
        } else {
            unlockTaskLock(&node->ref_lock);
        }
        return simpleError(SUCCESS);
    } else {
        Error err = simpleError(SUCCESS);
        size_t refs = vfsCacheCloseNode(&node->superblock->nodes, node);
        if (refs == 0) {
            VfsSuperblock* sb = node->superblock;
            if (sb->root_node == node) {
                // The root node is special. We never totally remove it until we close the superblock.
                // Still dereference the superblock, this might trigger freeing everything.
                vfsSuperClose(sb);
            } else {
                // If ref_count is 0 it was removed from the cache. We can free it.
                if (node->stat.nlinks == 0) {
                    // There are no other links to this node, we should free the data.
                    CHECKED(vfsSuperFreeNode(node));
                }
                if (node->dirty) {
                    err = node->superblock->functions->write_node(node->superblock, node);
                }
                node->functions->free(node);
                vfsSuperClose(sb);
            }
        }
        return err;
    }
}

void vfsSuperCopy(VfsSuperblock* sb) {
    lockTaskLock(&sb->ref_lock);
    sb->ref_count++;
    unlockTaskLock(&sb->ref_lock);
}

void vfsSuperClose(VfsSuperblock* sb) {
    lockTaskLock(&sb->ref_lock);
    sb->ref_count--;
    if (sb->ref_count == 0) {
        // If no reference is left, only the root node should remain (and that without references).
        // Also, at this point sb is not mounted anywhere anymore.
        assert(sb->root_node->ref_count == 0);
        assert(sb->nodes.count == 0);
        unlockTaskLock(&sb->ref_lock);
        vfsCacheDeinit(&sb->nodes);
        sb->root_node->functions->free(sb->root_node);
        sb->functions->free(sb);
    } else {
        unlockTaskLock(&sb->ref_lock);
    }
}

