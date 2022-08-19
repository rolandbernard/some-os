
#include "files/vfs/node.h"

#include "files/vfs/fs.h"
#include "files/vfs/super.h"

#define DELEGATE_NODE_FUNCTION(NAME, PARAMS, ACCESS)        \
    if (node->functions->NAME == NULL) {                    \
        return simpleError(EINVAL);                         \
    } else {                                                \
        lockTaskLock(&node->lock);                          \
        CHECKED(canAccess(&node->stat, process, ACCESS), {  \
            unlockTaskLock(&node->lock);                    \
        });                                                 \
        unlockTaskLock(&node->lock);                        \
        return node->functions->NAME PARAMS;                \
    }

Error vfsNodeReadAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    DELEGATE_NODE_FUNCTION(read_at, (node, buff, offset, length, read), VFS_ACCESS_R);
}

Error vfsNodeWriteAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* written) {
    DELEGATE_NODE_FUNCTION(write_at, (node, buff, offset, length, written), VFS_ACCESS_W);
}

Error vfsNodeReaddirAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    DELEGATE_NODE_FUNCTION(readdir_at, (node, buff, offset, length, read), VFS_ACCESS_R | VFS_ACCESS_DIR);
}

Error vfsNodeTrunc(VfsNode* node, Process* process, size_t length) {
    DELEGATE_NODE_FUNCTION(trunc, (node, length), VFS_ACCESS_W);
}

static Error vfsNodeBasicLookup(VfsNode* node, Process* process, const char* name, size_t* ret) {
    DELEGATE_NODE_FUNCTION(lookup, (node, name, ret), VFS_ACCESS_X | VFS_ACCESS_DIR);
}

Error vfsNodeLookup(VfsNode* node, Process* process, const char* name, VfsNode** ret) {
    size_t node_id;
    CHECKED(vfsNodeBasicLookup(node, process, name, &node_id));
    return vfsSuperReadNode(node->superblock, node_id, ret);
}

Error vfsNodeUnlink(VfsNode* node, Process* process, const char* name) {
    DELEGATE_NODE_FUNCTION(unlink, (node, name), VFS_ACCESS_W | VFS_ACCESS_DIR);
}

Error vfsNodeLink(VfsNode* node, Process* process, const char* name, VfsNode* entry) {
    if (node->functions->link == NULL) {
        return simpleError(EINVAL);
    } else {
        lockTaskLock(&node->lock);
        CHECKED(canAccess(&node->stat, process, VFS_ACCESS_W | VFS_ACCESS_DIR), {
            unlockTaskLock(&node->lock);
        });
        node->stat.nlinks++;
        unlockTaskLock(&node->lock);
        CHECKED(vfsSuperWriteNode(node));
        return node->functions->link(node, name, entry);
    }
}

void vfsNodeCopy(VfsNode* node) {
    vfsSuperCopyNode(node);
}

void vfsNodeClose(VfsNode* node) {
    vfsSuperCloseNode(node);
}

