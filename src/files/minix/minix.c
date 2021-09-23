
#include <stddef.h>
#include <string.h>

#include "files/minix/minix.h"

#include "error/log.h"
#include "files/minix/file.h"
#include "files/minix/maps.h"
#include "files/path.h"
#include "files/vfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode) {
    return (2 + fs->superblock.imap_blocks + fs->superblock.zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

typedef void (*MinixDirSearchCallback)(Error error, uint32_t inode, size_t offset, size_t file_size, void* udata);

typedef struct {
    MinixFile* file;
    Uid uid;
    Gid gid;
    const char* name;
    MinixDirSearchCallback callback;
    void* udata;
    size_t size;
    size_t file_size;
    size_t offset;
    size_t entry_count;
    MinixDirEntry* entries;
} MinixFindInDirectoryRequest;

#define MAX_SINGLE_READ_SIZE (1 << 16)

static void minixFindINodeInReadCallback(Error error, size_t read, MinixFindInDirectoryRequest* request);

static void readEntriesForRequest(MinixFindInDirectoryRequest* request) {
    size_t size = request->size;
    if (size < sizeof(MinixDirEntry)) {
        dealloc(request->entries);
        request->callback(simpleError(NO_SUCH_FILE), 0, request->offset, request->file_size, request->udata);
        dealloc(request);
    } else {
        if (size > MAX_SINGLE_READ_SIZE) {
            size = MAX_SINGLE_READ_SIZE;
        }
        request->size -= size;
        if (request->entries == NULL) {
            request->entries = kalloc(size);
        }
        request->entry_count = size / sizeof(MinixDirEntry);
        // Use uid 0 here because the system is traversing the file tree, not the user.
        // User can traverse with only x permissions, but not actually read or write.
        request->file->base.functions->read(
            (VfsFile*)request->file, 0, 0, virtPtrForKernel(request->entries), size,
            (VfsFunctionCallbackSizeT)minixFindINodeInReadCallback, request
        );
    }
}

static void minixFindINodeInReadCallback(Error error, size_t read, MinixFindInDirectoryRequest* request) {
    if (isError(error)) {
        dealloc(request->entries);
        request->callback(error, 0, request->offset, request->file_size, request->udata);
        dealloc(request);
    } else if (read == 0) {
        dealloc(request->entries);
        request->callback(simpleError(NO_SUCH_FILE), 0, request->offset, request->file_size, request->udata);
        dealloc(request);
    } else {
        for (size_t i = 0; i < request->entry_count; i++) {
            if (request->entries[i].inode != 0 && strcmp((char*)request->entries[i].name, request->name) == 0) {
                uint32_t inode = request->entries[i].inode;
                dealloc(request->entries);
                request->callback(simpleError(SUCCESS), inode, request->offset, request->file_size, request->udata);
                dealloc(request);
                return;
            }
            request->offset += sizeof(MinixDirEntry);
        }
        readEntriesForRequest(request);
    }
}

static void minixFindINodeInStatCallback(Error error, VfsStat stat, MinixFindInDirectoryRequest* request) {
    if (isError(error)) {
        request->callback(error, 0, 0, 0, request->udata);
        dealloc(request);
    } else if (MODE_TYPE(stat.mode) != VFS_TYPE_DIR) {
        // File is not a directory
        request->callback(simpleError(WRONG_FILE_TYPE), 0, 0, 0, request->udata);
        dealloc(request);
    } else if (!canAccess(stat.mode, stat.uid, stat.gid, request->uid, request->gid, VFS_ACCESS_X)) {
        request->callback(simpleError(FORBIDDEN), 0, 0, 0, request->udata);
        dealloc(request);
    } else {
        request->size = stat.size;
        request->file_size = stat.size;
        request->offset = 0;
        readEntriesForRequest(request);
    }
}

static void minixFindINodeForNameIn(MinixFile* file, Uid uid, Gid gid, const char* name, MinixDirSearchCallback callback, void* udata) {
    MinixFindInDirectoryRequest* request = kalloc(sizeof(MinixFindInDirectoryRequest));
    request->file = file;
    request->uid = uid;
    request->gid = gid;
    request->name = name;
    request->callback = callback;
    request->udata = udata;
    request->entries = NULL;
    file->base.functions->stat((VfsFile*)file, uid, gid, (VfsFunctionCallbackStat)minixFindINodeInStatCallback, request);
}

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    char* path;
    char* path_copy;
    MinixINodeCallback callback;
    void* udata;
    Error error;
    uint32_t inode;
    MinixFile* file;
} MinixFindINodeRequest;

static void minixFindINodeStepCallback(Error error, uint32_t inode, size_t offset, size_t size, MinixFindINodeRequest* request);

static void minixFindINodeCloseCallback(Error error, MinixFindINodeRequest* request) {
    if (isError(request->error)) {
        dealloc(request->path_copy);
        request->callback(request->error, 0, request->udata);
        dealloc(request);
    } else if (isError(error)) {
        dealloc(request->path_copy);
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (request->path[0] == 0) {
        dealloc(request->path_copy);
        request->callback(simpleError(SUCCESS), request->inode, request->udata);
        dealloc(request);
    } else {
        const char* name = request->path;
        while (*request->path != 0 && *request->path != '/') {
            request->path++;
        }
        if (*request->path == '/') {
            *request->path = 0;
            request->path++;
        }
        request->file = createMinixFileForINode(request->fs, request->inode);
        minixFindINodeForNameIn(
            request->file, request->uid, request->gid, name,
            (MinixDirSearchCallback)minixFindINodeStepCallback, request
        );
    }
}

static void minixFindINodeStepCallback(Error error, uint32_t inode, size_t offset, size_t size, MinixFindINodeRequest* request) {
    request->error = error;
    request->inode = inode;
    if (request->file != NULL) {
        request->file->base.functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixFindINodeCloseCallback,
            request
        );
    } else {
        minixFindINodeCloseCallback(simpleError(SUCCESS), request);
    }
}

static void minixFindINodeForPath(MinixFilesystem* fs, Uid uid, Gid gid, const char* path, MinixINodeCallback callback, void* udata) {
    MinixFindINodeRequest* request = kalloc(sizeof(MinixFindINodeRequest));
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->path_copy = reducedPathCopy(path);
    request->path = request->path_copy + (request->path_copy[0] == '/' ? 1 : 0); // all paths start with /, skip it
    request->callback = callback;
    request->udata = udata;
    request->file = NULL;
    minixFindINodeStepCallback(simpleError(SUCCESS), 1, 0, 0, request); // Start searching at the root node
}

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    VfsOpenFlags flags;
    VfsMode mode;
    VfsFunctionCallbackFile callback;
    char* path;
    void* udata;
    VfsFile* file;
    uint32_t inodenum;
    MinixInode inode;
    MinixDirEntry entry;
    Error error;
} MinixOpenRequest;

static void minixOpenCloseCallback(Error error, MinixOpenRequest* request) {
    request->callback(request->error, NULL, request->udata);
    dealloc(request);
}

static void minixOpenSeekCallback(Error error, size_t pos, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(SUCCESS), request->file, request->udata);
        dealloc(request);
    }
}

static void minixOpenTruncCallback(Error error, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(SUCCESS), request->file, request->udata);
        dealloc(request);
    }
}

static void minixOpenReadCallback(Error error, size_t read, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(IO_ERROR), NULL, request->udata);
        dealloc(request);
    } else if ((request->flags & VFS_OPEN_DIRECTORY) != 0 && MODE_TYPE(request->inode.mode) != VFS_TYPE_DIR) {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(WRONG_FILE_TYPE), NULL, request->udata);
        dealloc(request);
    } else if ((request->flags & VFS_OPEN_DIRECTORY) == 0 && MODE_TYPE(request->inode.mode) == VFS_TYPE_DIR) {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(WRONG_FILE_TYPE), NULL, request->udata);
        dealloc(request);
    } else if (!canAccess(request->inode.mode, request->inode.uid, request->inode.gid, request->uid, request->gid, OPEN_ACCESS(request->flags))) {
        unlockSpinLock(&request->fs->lock);
        request->callback(simpleError(FORBIDDEN), NULL, request->udata);
        dealloc(request);
    } else {
        MinixFile* file = createMinixFileForINode(request->fs, request->inodenum);
        request->file = (VfsFile*)file;
        if ((request->flags & VFS_OPEN_APPEND) != 0) {
            file->base.functions->seek(
                request->file, request->uid, request->gid, request->inode.size, VFS_SEEK_SET,
                (VfsFunctionCallbackSizeT)minixOpenSeekCallback, request
            );
        } else if ((request->flags & VFS_OPEN_TRUNC) != 0) {
            file->base.functions->trunc(
                (VfsFile*)file, request->uid, request->gid, 0,
                (VfsFunctionCallbackVoid)minixOpenTruncCallback, request
            );
        } else {
            unlockSpinLock(&request->fs->lock);
            request->callback(simpleError(SUCCESS), request->file, request->udata);
            dealloc(request);
        }
    }
}

static void minixOpenCreateDirCloseCallback(Error error, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        vfsReadAt(
            request->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, request->inodenum),
            (VfsFunctionCallbackSizeT)minixOpenReadCallback, request
        );
    }
}

static void minixOpenCreateWriteDirCallback(Error error, size_t read, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixOpenCreateDirCloseCallback, request
        );
    }
}

static void minixOpenWriteCallback(Error error, size_t read, MinixOpenRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else if (read != sizeof(MinixInode)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(IO_ERROR);
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        request->entry.inode = request->inodenum;
        size_t len = strlen(request->path) - 1;
        while (len > 0 && request->path[len] != '/') {
            len--;
        }
        size_t name_len = strlen(request->path + len + 1) + 1;
        if (name_len > sizeof(request->entry.name)) {
            name_len = sizeof(request->entry.name);
        }
        memcpy(request->entry.name, request->path + len + 1, name_len);
        dealloc(request->path);
        request->file->functions->write(
            request->file, request->uid, request->gid, virtPtrForKernel(&request->entry),
            sizeof(MinixDirEntry), (VfsFunctionCallbackSizeT)minixOpenCreateWriteDirCallback,
            request
        );
    }
}

static void minixOpenGetINodeCallback(Error error, uint32_t inode, MinixOpenRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        request->inodenum = inode;
        request->inode.atime = getUnixTime();
        request->inode.ctime = getUnixTime();
        request->inode.mtime = getUnixTime();
        request->inode.gid = request->gid;
        request->inode.mode =
            (request->mode & ~VFS_MODE_TYPE)
            | ((request->flags & VFS_OPEN_DIRECTORY) != 0 ? TYPE_MODE(VFS_TYPE_DIR) : TYPE_MODE(VFS_TYPE_REG));
        request->inode.nlinks = 1;
        request->inode.size = 0;
        request->inode.uid = request->uid;
        memset(request->inode.zones, 0, sizeof(request->inode.zones));
        vfsWriteAt(
            request->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, inode), (VfsFunctionCallbackSizeT)minixOpenWriteCallback,
            request
        );
    }
}

static void minixOpenParentINodeCallback(Error error, VfsFile* file, MinixOpenRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        lockSpinLock(&request->fs->lock);
        request->file = file;
        getFreeMinixInode(request->fs, (MinixINodeCallback)minixOpenGetINodeCallback, request);
    }
}

static void minixOpenINodeCallback(Error error, uint32_t inode, MinixOpenRequest* request) {
    if (request->path[1] != 0 && error.kind == NO_SUCH_FILE && (request->flags & VFS_OPEN_CREATE) != 0) {
        // If we don't find the node itself, try opening the parent
        char* path = reducedPathCopy(request->path);
        size_t len = strlen(path) - 1;
        while (len > 0 && path[len] != '/') {
            len--;
        }
        if (len == 0) {
            len++;
        }
        path[len] = 0;
        unlockSpinLock(&request->fs->lock);
        request->fs->base.functions->open(
            (VfsFilesystem*)request->fs, request->uid, request->gid, path,
            VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0,
            (VfsFunctionCallbackFile)minixOpenParentINodeCallback, request
        );
        dealloc(path);
    } else if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        dealloc(request->path);
        request->inodenum = inode;
        vfsReadAt(
            request->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, inode), (VfsFunctionCallbackSizeT)minixOpenReadCallback,
            request
        );
    }
}

static void minixOpenFunction(
    MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode,
    VfsFunctionCallbackFile callback, void* udata
) {
    MinixOpenRequest* request = kalloc(sizeof(MinixOpenRequest));
    lockSpinLock(&fs->lock);
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->path = reducedPathCopy(path);
    request->flags = flags;
    request->mode = mode;
    request->callback = callback;
    request->udata = udata;
    minixFindINodeForPath(fs, uid, gid, path, (MinixINodeCallback)minixOpenINodeCallback, request);
}

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    char* path;
    VfsFunctionCallbackVoid callback;
    void* udata;
    VfsFile* dir;
    VfsFile* file;
    size_t offset;
    size_t dir_size;
    Error error;
    uint32_t inodenum;
    MinixDirEntry entry;
    MinixInode inode;
} MinixUnlinkRequest;

static void minixUnlinkDirCloseCallback(Error error, MinixUnlinkRequest* request) {
    request->callback(request->error, request->udata);
    dealloc(request);
}

static void minixUnlinkFileErrorCloseCallback(Error error, MinixUnlinkRequest* request) {
    request->dir->functions->close(
        (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
    );
}

static void minixUnlinkDirectoryTruncCallback(Error error, MinixUnlinkRequest* request) {
    unlockSpinLock(&request->fs->lock);
    if (isError(error)) {
        request->error = error;
    } else {
        request->error = simpleError(SUCCESS);
    }
    request->dir->functions->close(
        (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
    );
}

static void minixUnlinkDirectoryWriteCallback(Error error, size_t read, MinixUnlinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else if (read != sizeof(MinixDirEntry)) {
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(IO_ERROR);
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else {
        request->dir->functions->trunc(
            request->dir, request->uid, request->gid, request->dir_size - sizeof(MinixDirEntry),
            (VfsFunctionCallbackVoid)minixUnlinkDirectoryTruncCallback, request
        );
    }
}

static void minixUnlinkDirectoryReadCallback(Error error, size_t read, MinixUnlinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else if (read != sizeof(MinixDirEntry)) {
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(IO_ERROR);
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else {
        vfsWriteAt(
            request->dir, request->uid, request->gid, virtPtrForKernel(&request->entry), sizeof(MinixDirEntry),
            request->offset, (VfsFunctionCallbackSizeT)minixUnlinkDirectoryWriteCallback, request
        );
    }
}

static void minixUnlinkRemoveEntry(MinixUnlinkRequest* request) {
    // Both paths (decrease nlinks and free space) merge here
    vfsReadAt(
        request->dir, request->uid, request->gid, virtPtrForKernel(&request->entry), sizeof(MinixDirEntry),
        request->dir_size - sizeof(MinixDirEntry),
        (VfsFunctionCallbackSizeT)minixUnlinkDirectoryReadCallback, request
    );
}

static void minixUnlinkWriteCallback(Error error, size_t read, MinixUnlinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else if (read != sizeof(MinixInode)) {
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(IO_ERROR);
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else {
        minixUnlinkRemoveEntry(request);
    }
}

static void minixUnlinkFreeInodeCallback(Error error, MinixUnlinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(IO_ERROR);
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else {
        minixUnlinkRemoveEntry(request);
    }
}

static void minixUnlinkFileCloseCallback(Error error, MinixUnlinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(IO_ERROR);
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else {
        lockSpinLock(&request->fs->lock);
        freeMinixInode(request->fs, request->inodenum, (VfsFunctionCallbackVoid)minixUnlinkFreeInodeCallback, request);
    }
}

static void minixUnlinkTruncCallback(Error error, MinixUnlinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            request->file, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkFileErrorCloseCallback, request
        );
    } else {
        unlockSpinLock(&request->fs->lock);
        request->file->functions->close(
            request->file, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkFileCloseCallback, request
        );
    }
}

static void minixUnlinkGetINodeCallback(Error error, size_t read, MinixUnlinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->dir->functions->close(
            (VfsFile*)request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else {
        if (request->inode.nlinks == 1) {
            request->file = (VfsFile*)createMinixFileForINode(request->fs, request->inodenum);
            request->file->functions->trunc(
                request->file, 0, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkTruncCallback, request
            );
        } else {
            request->inode.nlinks--;
            vfsWriteAt(
                request->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
                offsetForINode(request->fs, request->inodenum),
                (VfsFunctionCallbackSizeT)minixUnlinkWriteCallback, request
            );
        }
    }
}

static void minixUnlinkFindEntryStepCallback(Error error, uint32_t inode, size_t offset, size_t size, MinixUnlinkRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->dir->functions->close(
            request->dir, 0, 0, (VfsFunctionCallbackVoid)minixUnlinkDirCloseCallback, request
        );
    } else {
        dealloc(request->path);
        request->inodenum = inode;
        request->dir_size = size;
        request->offset = offset;
        vfsReadAt(
            request->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, request->inodenum),
            (VfsFunctionCallbackSizeT)minixUnlinkGetINodeCallback, request
        );
    }
}

static void minixUnlinkOpenParentCallback(Error error, VfsFile* file, MinixUnlinkRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        lockSpinLock(&request->fs->lock);
        request->dir = file;
        size_t len = strlen(request->path) - 1;
        while (len > 0 && request->path[len] != '/') {
            len--;
        }
        minixFindINodeForNameIn(
            (MinixFile*)request->dir, request->uid, request->gid, request->path + len + 1,
            (MinixDirSearchCallback)minixUnlinkFindEntryStepCallback, request
        );
    }
}

static void minixUnlinkFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
    MinixUnlinkRequest* request = kalloc(sizeof(MinixUnlinkRequest));
    lockSpinLock(&fs->lock);
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->path = reducedPathCopy(path);
    request->callback = callback;
    request->udata = udata;
    // Try opening the parent
    char* path_copy = reducedPathCopy(request->path);
    size_t len = strlen(path_copy) - 1;
    while (len > 0 && path_copy[len] != '/') {
        len--;
    }
    if (len == 0) {
        len++;
    }
    path_copy[len] = 0;
    unlockSpinLock(&request->fs->lock);
    request->fs->base.functions->open(
        (VfsFilesystem*)request->fs, request->uid, request->gid, path_copy,
        VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0,
        (VfsFunctionCallbackFile)minixUnlinkOpenParentCallback, request
    );
    dealloc(path_copy);
}

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    char* old;
    char* path;
    VfsFunctionCallbackVoid callback;
    void* udata;
    VfsFile* file;
    Error error;
    uint32_t inodenum;
    MinixDirEntry entry;
    MinixInode inode;
} MinixLinkRequest;

static void minixLinkDirCloseCallback(Error error, MinixLinkRequest* request) {
    request->callback(request->error, request->udata);
    dealloc(request);
}

static void minixLinkWriteDirCallback(Error error, size_t read, MinixLinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixLinkDirCloseCallback, request
        );
    } else {
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(SUCCESS);
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixLinkDirCloseCallback, request
        );
    }
}

static void minixLinkWriteCallback(Error error, size_t read, MinixLinkRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixLinkDirCloseCallback, request
        );
    } else if (read != sizeof(MinixInode)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->error = simpleError(IO_ERROR);
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixLinkDirCloseCallback, request
        );
    } else {
        request->entry.inode = request->inodenum;
        size_t len = strlen(request->path) - 1;
        while (len > 0 && request->path[len] != '/') {
            len--;
        }
        size_t name_len = strlen(request->path + len + 1) + 1;
        if (name_len > sizeof(request->entry.name)) {
            name_len = sizeof(request->entry.name);
        }
        memcpy(request->entry.name, request->path + len + 1, name_len);
        dealloc(request->path);
        request->file->functions->write(
            request->file, request->uid, request->gid, virtPtrForKernel(&request->entry),
            sizeof(MinixDirEntry), (VfsFunctionCallbackSizeT)minixLinkWriteDirCallback,
            request
        );
    }
}

static void minixLinkGetINodeCallback(Error error, size_t read, MinixLinkRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, 0, 0, (VfsFunctionCallbackVoid)minixLinkDirCloseCallback, request
        );
    } else {
        request->inode.nlinks++;
        vfsWriteAt(
            request->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, request->inodenum),
            (VfsFunctionCallbackSizeT)minixLinkWriteCallback, request
        );
    }
}

static void minixLinkParentINodeCallback(Error error, VfsFile* file, MinixLinkRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockSpinLock(&request->fs->lock);
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        lockSpinLock(&request->fs->lock);
        request->file = file;
        vfsReadAt(
            request->fs->block_device, 0, 0, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, request->inodenum),
            (VfsFunctionCallbackSizeT)minixLinkGetINodeCallback, request
        );
    }
}

static void minixLinkFindINodeCallback(Error error, uint32_t inode, MinixLinkRequest* request) {
    if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        dealloc(request->path);
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        request->inodenum = inode;
        // Try opening the parent
        char* path = reducedPathCopy(request->path);
        size_t len = strlen(path) - 1;
        while (len > 0 && path[len] != '/') {
            len--;
        }
        if (len == 0) {
            len++;
        }
        path[len] = 0;
        unlockSpinLock(&request->fs->lock);
        request->fs->base.functions->open(
            (VfsFilesystem*)request->fs, request->uid, request->gid, path,
            VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0,
            (VfsFunctionCallbackFile)minixLinkParentINodeCallback, request
        );
        dealloc(path);
    }
}

static void minixLinkFindNewINodeCallback(Error error, uint32_t inode, MinixLinkRequest* request) {
    if (error.kind == NO_SUCH_FILE) {
        minixFindINodeForPath(request->fs, request->uid, request->gid, request->old, (MinixINodeCallback)minixLinkFindINodeCallback, request);
        dealloc(request->old);
    } else if (isError(error)) {
        unlockSpinLock(&request->fs->lock);
        dealloc(request->old);
        dealloc(request->path);
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        unlockSpinLock(&request->fs->lock);
        dealloc(request->old);
        dealloc(request->path);
        request->callback(simpleError(ALREADY_IN_USE), request->udata);
        dealloc(request);
    }
}

static void minixLinkFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    MinixLinkRequest* request = kalloc(sizeof(MinixLinkRequest));
    lockSpinLock(&fs->lock);
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->old = reducedPathCopy(old);
    request->path = reducedPathCopy(new);
    request->callback = callback;
    request->udata = udata;
    minixFindINodeForPath(fs, uid, gid, new, (MinixINodeCallback)minixLinkFindNewINodeCallback, request);
}

typedef struct {
    MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    const char* old;
    VfsFunctionCallbackVoid callback;
    void* udata;
} MinixRenameRequest;

static void minixRenameLinkCallback(Error error, MinixRenameRequest* request) {
    if (isError(error)) {
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        minixUnlinkFunction(
            request->fs, request->uid, request->gid, request->old, request->callback, request->udata
        );
        dealloc(request);
    }
}

static void minixRenameFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    // Implementation is not ideal, but simpler
    MinixRenameRequest* request = kalloc(sizeof(MinixRenameRequest));
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->old = old;
    request->callback = callback;
    request->udata = udata;
    minixLinkFunction(
        fs, uid, gid, old, new, (VfsFunctionCallbackVoid)minixRenameLinkCallback, request
    );
}

static void minixFreeFunction(MinixFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    // We don't do any caching. Otherwise write it to disk here.
    dealloc(fs);
    callback(simpleError(SUCCESS), udata);
}

typedef struct {
    MinixFilesystem* fs;
    VfsFunctionCallbackVoid callback;
    void* udata;
} MinixInitRequest;

static void minixSuperblockReadCallback(Error error, size_t read, MinixInitRequest* request) {
    if (isError(error)) {
        request->callback(error, request->udata);
        dealloc(request);
    } else if (read != sizeof(Minix3Superblock)) {
        request->callback(simpleError(IO_ERROR), request->udata);
        dealloc(request);
    } else if (request->fs->superblock.magic != MINIX3_MAGIC) {
        request->callback(simpleError(WRONG_FILE_TYPE), request->udata);
        dealloc(request);
    } else {
        // Don't read more for now
        request->callback(simpleError(SUCCESS), request->udata);
        dealloc(request);
    }
}

static void minixInitFunction(MinixFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    MinixInitRequest* request = kalloc(sizeof(MinixInitRequest));
    request->fs = fs;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        fs->block_device, 0, 0, virtPtrForKernel(&fs->superblock), sizeof(Minix3Superblock),
        MINIX_BLOCK_SIZE, (VfsFunctionCallbackSizeT)minixSuperblockReadCallback, request
    );
}

static const VfsFilesystemVtable functions = {
    .open = (OpenFunction)minixOpenFunction,
    .unlink = (UnlinkFunction)minixUnlinkFunction,
    .link = (LinkFunction)minixLinkFunction,
    .rename = (RenameFunction)minixRenameFunction,
    .free = (FreeFunction)minixFreeFunction,
    .init = (InitFunction)minixInitFunction,
};

MinixFilesystem* createMinixFilesystem(VfsFile* block_device, void* data) {
    MinixFilesystem* file = zalloc(sizeof(MinixFilesystem));
    file->base.functions = &functions;
    file->block_device = block_device;
    return file;
}

