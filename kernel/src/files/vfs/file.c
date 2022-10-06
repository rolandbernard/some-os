
#include "files/vfs/file.h"

#include "files/vfs/fs.h"
#include "files/vfs/node.h"
#include "files/vfs/super.h"
#include "memory/kalloc.h"
#include "util/util.h"

Error vfsFileSeek(VfsFile* file, Process* process, size_t offset, VfsSeekWhence whence, size_t* new_pos) {
    lockTaskLock(&file->lock);
    if (whence == VFS_SEEK_CUR) {
        file->offset = file->offset + offset;
        *new_pos = file->offset;
    } else if (whence == VFS_SEEK_SET) {
        file->offset = offset;
        *new_pos = file->offset;
    } else if (whence == VFS_SEEK_END) {
        lockTaskLock(&file->node->lock);
        file->offset = file->node->stat.size + offset;
        *new_pos = file->offset;
        unlockTaskLock(&file->node->lock);
    } else {
        unlockTaskLock(&file->lock);
        return simpleError(EINVAL);
    }
    unlockTaskLock(&file->lock);
    return simpleError(SUCCESS);
}

Error vfsFileRead(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* read) {
    lockTaskLock(&file->lock);
    Error err = vfsFileReadAt(file, process, buffer, file->offset, length, read);
    if (!isError(err)) {
        file->offset += *read;
    }
    unlockTaskLock(&file->lock);
    return err;
}

Error vfsFileWrite(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* written) {
    lockTaskLock(&file->lock);
    Error err = vfsFileWriteAt(file, process, buffer, file->offset, length, written);
    if (!isError(err)) {
        file->offset += *written;
    }
    unlockTaskLock(&file->lock);
    return err;
}

Error vfsFileReadAt(VfsFile* file, Process* process, VirtPtr buffer, size_t offset, size_t length, size_t* read) {
    return vfsNodeReadAt(file->node, process, buffer, offset, length, read, (file->flags & VFS_FILE_NONBLOCK) == 0);
}

Error vfsFileWriteAt(VfsFile* file, Process* process, VirtPtr buffer, size_t offset, size_t length, size_t* written) {
    return vfsNodeWriteAt(file->node, process, buffer, offset, length, written, (file->flags & VFS_FILE_NONBLOCK) == 0);
}

Error vfsFileStat(VfsFile* file, Process* process, VirtPtr ret) {
    lockTaskLock(&file->node->lock);
    memcpyBetweenVirtPtr(ret, virtPtrForKernel(&file->node->real_node->stat), sizeof(VfsStat));
    unlockTaskLock(&file->node->lock);
    return simpleError(SUCCESS);
}

Error vfsFileTrunc(VfsFile* file, Process* process, size_t size) {
    return vfsNodeTrunc(file->node, process, size);
}

Error vfsFileChmod(VfsFile* file, Process* process, VfsMode mode) {
    // chmod and chown should use the real node.
    VfsNode* node = file->node->real_node;
    lockTaskLock(&node->lock);
    CHECKED(canAccess(node, process, VFS_ACCESS_CHMOD), {
        unlockTaskLock(&node->lock);
    });
    node->stat.mode &= VFS_MODE_TYPE;
    node->stat.mode |= mode & ~VFS_MODE_TYPE; // We don't allow changing the type
    Error err = vfsSuperWriteNode(node);
    unlockTaskLock(&node->lock);
    return err;
}

Error vfsFileChown(VfsFile* file, Process* process, Uid uid, Gid gid) {
    VfsNode* node = file->node->real_node;
    lockTaskLock(&node->lock);
    CHECKED(canAccess(node, process, VFS_ACCESS_CHOWN), {
        unlockTaskLock(&node->lock);
    });
    if (uid >= 0) {
        node->stat.uid = uid;
    }
    if (gid >= 0) {
        node->stat.gid = gid;
    }
    Error err = vfsSuperWriteNode(node);
    unlockTaskLock(&node->lock);
    return err;
}

Error vfsFileReaddir(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* read) {
    lockTaskLock(&file->lock);
    size_t tmp_size;
    Error err = vfsNodeReaddirAt(file->node, process, buffer, file->offset, length, &tmp_size, read);
    if (!isError(err)) {
        file->offset += tmp_size;
    }
    unlockTaskLock(&file->lock);
    return err;
}

Error vfsFileIoctl(VfsFile* file, Process* process, size_t request, VirtPtr argp, uintptr_t* out) {
    return vfsNodeIoctl(file->node, process, request, argp, out);
}

bool vfsFileIsReady(VfsFile* file, Process* process, bool write) {
    return vfsNodeIsReady(file->node, process, write);
}

void vfsFileCopy(VfsFile* file) {
    lockTaskLock(&file->ref_lock);
    file->ref_count++;
    unlockTaskLock(&file->ref_lock);
}

void vfsFileClose(VfsFile* file) {
    lockTaskLock(&file->ref_lock);
    file->ref_count--;
    if (file->ref_count == 0) {
        unlockTaskLock(&file->ref_lock);
        vfsNodeClose(file->node);
        dealloc(file);
    } else {
        unlockTaskLock(&file->ref_lock);
    }
}

