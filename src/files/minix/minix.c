
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

typedef struct {
    MinixFile* file;
    Uid uid;
    Gid gid;
    const char* name;
    MinixINodeCallback callback;
    void* udata;
    size_t size;
    size_t entry_count;
    MinixDirEntry* entries;
} MinixFindInDirectoryRequest;

#define MAX_SINGLE_READ_SIZE (1 << 16)

static void minixFindINodeInReadCallback(Error error, size_t read, MinixFindInDirectoryRequest* request);

static void readEntriesForRequest(MinixFindInDirectoryRequest* request) {
    size_t size = request->size;
    if (size < sizeof(MinixDirEntry)) {
        dealloc(request->entries);
        request->callback(simpleError(NO_SUCH_FILE), 0, request->udata);
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
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read == 0) {
        dealloc(request->entries);
        request->callback(simpleError(NO_SUCH_FILE), 0, request->udata);
        dealloc(request);
    } else {
        for (size_t i = 0; i < request->entry_count; i++) {
            if (request->entries[i].inode != 0 && strcmp((char*)request->entries[i].name, request->name) == 0) {
                uint32_t inode = request->entries[i].inode;
                dealloc(request->entries);
                request->callback(simpleError(SUCCESS), inode, request->udata);
                dealloc(request);
                return;
            }
        }
        readEntriesForRequest(request);
    }
}

static void minixFindINodeInStatCallback(Error error, VfsStat stat, MinixFindInDirectoryRequest* request) {
    if (isError(error)) {
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (MODE_TYPE(stat.mode) != VFS_TYPE_DIR) {
        // File is not a directory
        request->callback(simpleError(WRONG_FILE_TYPE), 0, request->udata);
        dealloc(request);
    } else if (!canAccess(stat.mode, stat.uid, stat.gid, request->uid, request->gid, VFS_ACCESS_X)) {
        request->callback(simpleError(FORBIDDEN), 0, request->udata);
        dealloc(request);
    } else {
        request->size = stat.size;
        readEntriesForRequest(request);
    }
}

static void minixFindINodeForNameIn(MinixFile* file, Uid uid, Gid gid, const char* name, MinixINodeCallback callback, void* udata) {
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

static void minixFindINodeStepCallback(Error error, uint32_t inode, MinixFindINodeRequest* request);

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
            (MinixINodeCallback)minixFindINodeStepCallback, request
        );
    }
}

static void minixFindINodeStepCallback(Error error, uint32_t inode, MinixFindINodeRequest* request) {
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
    minixFindINodeStepCallback(simpleError(SUCCESS), 1, request); // Start searching at the root node
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

static void minixUnlinkFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
    // TODO: implement
    // Find parent inode
    // Find entry
    // Save inode
    // Overrite entry with last
    // Truncate directory file
    // If nlinks = 1
    //   Truncate file to 0
    //   Free inode
    // Else
    //   Write data to inode (decrease nlinks)
}

static void minixLinkFunction(MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    // TODO: implement
    // (Similar to create)
    // Find inode for old
    // Write data to inode (increase nlinks)
    // Link inode
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
    // TODO: Optimize
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

