
#include "files/vfs/file.h"

#include "files/special/blkfile.h"
#include "files/special/pipe.h"
#include "files/special/ttyfile.h"
#include "files/vfs/fs.h"
#include "files/vfs/node.h"
#include "files/vfs/super.h"
#include "memory/kalloc.h"

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
    Error err = vfsFileReadAt(file, process, buffer, length, file->offset, read);
    if (!isError(err)) {
        file->offset += *read;
    }
    unlockTaskLock(&file->lock);
    return err;
}

Error vfsFileWrite(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* written) {
    lockTaskLock(&file->lock);
    Error err = vfsFileWriteAt(file, process, buffer, length, file->offset, written);
    if (!isError(err)) {
        file->offset += *written;
    }
    unlockTaskLock(&file->lock);
    return err;
}

Error vfsFileReadAt(VfsFile* file, Process* process, VirtPtr buffer, size_t offset, size_t length, size_t* read) {
    return vfsNodeReadAt(file->node, process, buffer, offset, length, read);
}

Error vfsFileWriteAt(VfsFile* file, Process* process, VirtPtr buffer, size_t offset, size_t length, size_t* written) {
    return vfsNodeWriteAt(file->node, process, buffer, offset, length, written);
}

Error vfsFileStat(VfsFile* file, Process* process, VirtPtr ret) {
    lockTaskLock(&file->node->lock);
    memcpyBetweenVirtPtr(ret, virtPtrForKernel(&file->node->stat), sizeof(VfsStat));
    unlockTaskLock(&file->node->lock);
    return simpleError(SUCCESS);
}

Error vfsFileTrunc(VfsFile* file, Process* process, size_t size) {
    return vfsNodeTrunc(file->node, process, size);
}

Error vfsFileChmod(VfsFile* file, Process* process, VfsMode mode) {
    lockTaskLock(&file->node->lock);
    CHECKED(canAccess(&file->node->stat, process, VFS_ACCESS_CHMOD), {
        unlockTaskLock(&file->node->lock);
    });
    file->node->stat.mode &= VFS_MODE_TYPE;
    file->node->stat.mode |= mode & ~VFS_MODE_TYPE; // We don't allow changing the type
    Error err = vfsSuperWriteNode(file->node);
    unlockTaskLock(&file->node->lock);
    return err;
}

Error vfsFileChown(VfsFile* file, Process* process, Uid uid, Gid gid) {
    lockTaskLock(&file->node->lock);
    file->node->stat.uid = uid;
    file->node->stat.gid = gid;
    Error err = vfsSuperWriteNode(file->node);
    unlockTaskLock(&file->node->lock);
    return err;
}

Error vfsFileReaddir(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* read) {
    lockTaskLock(&file->lock);
    Error err = vfsNodeReaddirAt(file->node, process, buffer, length, file->offset, read);
    if (!isError(err)) {
        file->offset += *read;
    }
    unlockTaskLock(&file->lock);
    return err;
}

void vfsFileCopy(VfsFile* file) {
    lockTaskLock(&file->lock);
    file->ref_count++;
    unlockTaskLock(&file->lock);
}

void vfsFileClose(VfsFile* file) {
    lockTaskLock(&file->lock);
    file->ref_count--;
    if (file->ref_count == 0) {
        unlockTaskLock(&file->lock);
        vfsNodeClose(file->node);
        dealloc(file);
    } else {
        unlockTaskLock(&file->lock);
    }
}

