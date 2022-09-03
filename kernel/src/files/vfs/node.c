
#include "files/vfs/node.h"

#include "files/vfs/fs.h"
#include "files/vfs/super.h"
#include "kernel/time.h"

#define DELEGATE_NODE_FUNCTION(NAME, PARAMS, ACCESS)                    \
    if (node->functions->NAME == NULL) {                                \
        return simpleError(EINVAL);                                     \
    } else {                                                            \
        lockTaskLock(&node->lock);                                      \
        CHECKED(                                                        \
            canAccess(node, process, ACCESS),                           \
            unlockTaskLock(&node->lock)                                 \
        );                                                              \
        Time time = getNanoseconds();                                   \
        if (((ACCESS) & VFS_ACCESS_W) != 0) {                           \
            node->stat.mtime = time;                                    \
        }                                                               \
        node->stat.atime = time;                                        \
        CHECKED(vfsSuperWriteNode(node), unlockTaskLock(&node->lock));  \
        unlockTaskLock(&node->lock);                                    \
        return node->functions->NAME PARAMS;                            \
    }

Error vfsNodeReadAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    DELEGATE_NODE_FUNCTION(read_at, (node, buff, offset, length, read), VFS_ACCESS_R);
}

Error vfsNodeWriteAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* written) {
    DELEGATE_NODE_FUNCTION(write_at, (node, buff, offset, length, written), VFS_ACCESS_W);
}

Error vfsNodeReaddirAt(VfsNode* node, Process* process, VirtPtr buff, size_t offset, size_t length, size_t* read_file, size_t* written_buff) {
    DELEGATE_NODE_FUNCTION(readdir_at, (node, buff, offset, length, read_file, written_buff), VFS_ACCESS_R | VFS_ACCESS_DIR);
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

static Error vfsNodeBasicUnlink(VfsNode* node, Process* process, const char* name) {
    DELEGATE_NODE_FUNCTION(unlink, (node, name), VFS_ACCESS_W | VFS_ACCESS_DIR);
}

Error vfsNodeUnlink(VfsNode* node, Process* process, const char* name, VfsNode* entry) {
    // For link and unlink, we should consider the real node for special files.
    // node can't be a special file (as that will give an EXDEV error).
    entry = entry->real_node;
    if (node->superblock != entry->superblock) {
        return simpleError(EXDEV);
    } else {
        lockTaskLock(&node->lock);
        if ((node->stat.mode & VFS_MODE_STICKY) != 0) {
            lockTaskLock(&entry->lock);
            CHECKED(canAccess(entry, process, VFS_ACCESS_W), {
                unlockTaskLock(&entry->lock);
                unlockTaskLock(&node->lock);
            });
            unlockTaskLock(&entry->lock);
        }
        CHECKED(vfsNodeBasicUnlink(node, process, name), unlockTaskLock(&node->lock));
        unlockTaskLock(&node->lock);
        lockTaskLock(&entry->lock);
        entry->stat.nlinks -= 1;
        Error err = vfsSuperWriteNode(entry);
        unlockTaskLock(&entry->lock);
        return err;
    }
}

Error vfsNodeLink(VfsNode* node, Process* process, const char* name, VfsNode* entry) {
    entry = entry->real_node;
    if (node->superblock != entry->superblock) {
        return simpleError(EXDEV);
    } else if (node->functions->link == NULL) {
        return simpleError(EINVAL);
    } else {
        lockTaskLock(&node->lock);
        CHECKED(
            canAccess(node, process, VFS_ACCESS_W | VFS_ACCESS_DIR),
            unlockTaskLock(&node->lock)
        );
        Time time = getNanoseconds();
        node->stat.mtime = time;
        node->stat.atime = time;
        CHECKED(vfsSuperWriteNode(node), unlockTaskLock(&node->lock));
        unlockTaskLock(&node->lock);
        lockTaskLock(&entry->lock);
        entry->stat.nlinks++;
        CHECKED(vfsSuperWriteNode(entry), unlockTaskLock(&entry->lock));
        unlockTaskLock(&entry->lock);
        return node->functions->link(node, name, entry);
    }
}

Error vfsNodeIoctl(VfsNode* node, Process* process, size_t request, VirtPtr argp, int* out) {
    if (node->functions->ioctl == NULL) {
        return simpleError(ENOTTY);
    } else {
        return node->functions->ioctl(node, request, argp, out);
    }
}

void vfsNodeCopy(VfsNode* node) {
    vfsSuperCopyNode(node);
}

void vfsNodeClose(VfsNode* node) {
    vfsSuperCloseNode(node);
}

