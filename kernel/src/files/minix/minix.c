
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
#include "util/util.h"

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode) {
    return (2 + fs->superblock.imap_blocks + fs->superblock.zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

typedef void (*MinixDirSearchCallback)(Error error, uint32_t inode, size_t offset, size_t file_size, void* udata);

typedef struct {
    MinixFile* file;
    Process* process;
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
        request->callback(simpleError(ENOENT), 0, request->offset, request->file_size, request->udata);
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
            (VfsFile*)request->file, NULL, virtPtrForKernel(request->entries), size,
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
        request->callback(simpleError(ENOENT), 0, request->offset, request->file_size, request->udata);
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
        request->callback(simpleError(ENOTDIR), 0, 0, 0, request->udata);
        dealloc(request);
    } else if (!canAccess(stat.mode, stat.uid, stat.gid, request->process, VFS_ACCESS_X)) {
        request->callback(simpleError(EACCES), 0, 0, 0, request->udata);
        dealloc(request);
    } else {
        request->size = stat.size;
        request->file_size = stat.size;
        request->offset = 0;
        readEntriesForRequest(request);
    }
}

static void minixFindINodeForNameIn(MinixFile* file, Process* process, const char* name, MinixDirSearchCallback callback, void* udata) {
    MinixFindInDirectoryRequest* request = kalloc(sizeof(MinixFindInDirectoryRequest));
    request->file = file;
    request->process = process;
    request->name = name;
    request->callback = callback;
    request->udata = udata;
    request->entries = NULL;
    file->base.functions->stat((VfsFile*)file, NULL, (VfsFunctionCallbackStat)minixFindINodeInStatCallback, request);
}

typedef struct {
    MinixFilesystem* fs;
    Process* process;
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
        request->file = createMinixFileForINode(request->fs, request->inode, true);
        minixFindINodeForNameIn(
            request->file, request->process, name,
            (MinixDirSearchCallback)minixFindINodeStepCallback, request
        );
    }
}

static void minixFindINodeStepCallback(Error error, uint32_t inode, size_t offset, size_t size, MinixFindINodeRequest* request) {
    request->error = error;
    request->inode = inode;
    if (request->file != NULL) {
        request->file->base.functions->close(
            (VfsFile*)request->file, NULL, (VfsFunctionCallbackVoid)minixFindINodeCloseCallback, request
        );
    } else {
        minixFindINodeCloseCallback(simpleError(SUCCESS), request);
    }
}

static void minixFindINodeForPath(MinixFilesystem* fs, Process* process, const char* path, MinixINodeCallback callback, void* udata) {
    MinixFindINodeRequest* request = kalloc(sizeof(MinixFindINodeRequest));
    request->fs = fs;
    request->process = process;
    request->path_copy = stringClone(path);
    request->path = request->path_copy + (request->path_copy[0] == '/' ? 1 : 0); // all paths start with /, skip it
    request->callback = callback;
    request->udata = udata;
    request->file = NULL;
    minixFindINodeStepCallback(simpleError(SUCCESS), 1, 0, 0, request); // Start searching at the root node
}

typedef struct {
    MinixFilesystem* fs;
    Process* process;
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
        unlockTaskLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, NULL, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        unlockTaskLock(&request->fs->lock);
        request->callback(simpleError(SUCCESS), request->file, request->udata);
        dealloc(request);
    }
}

static void minixOpenTruncCallback(Error error, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockTaskLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, NULL, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        unlockTaskLock(&request->fs->lock);
        request->callback(simpleError(SUCCESS), request->file, request->udata);
        dealloc(request);
    }
}

static void minixOpenReadCallback(Error error, size_t read, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockTaskLock(&request->fs->lock);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        unlockTaskLock(&request->fs->lock);
        request->callback(simpleError(EIO), NULL, request->udata);
        dealloc(request);
    } else if ((request->flags & VFS_OPEN_DIRECTORY) != 0 && MODE_TYPE(request->inode.mode) != VFS_TYPE_DIR) {
        unlockTaskLock(&request->fs->lock);
        request->callback(simpleError(ENOTDIR), NULL, request->udata);
        dealloc(request);
    } else if ((request->flags & VFS_OPEN_REGULAR) != 0 && MODE_TYPE(request->inode.mode) != VFS_TYPE_REG) {
        unlockTaskLock(&request->fs->lock);
        request->callback(simpleError(EISDIR), NULL, request->udata);
        dealloc(request);
    } else if (!canAccess(request->inode.mode, request->inode.uid, request->inode.gid, request->process, OPEN_ACCESS(request->flags))) {
        unlockTaskLock(&request->fs->lock);
        request->callback(simpleError(EACCES), NULL, request->udata);
        dealloc(request);
    } else {
        MinixFile* file = createMinixFileForINode(request->fs, request->inodenum, (request->flags & VFS_OPEN_DIRECTORY) != 0);
        file->base.mode = request->inode.mode;
        file->base.uid = request->inode.uid;
        file->base.gid = request->inode.gid;
        request->file = (VfsFile*)file;
        if ((request->flags & VFS_OPEN_APPEND) != 0) {
            file->base.functions->seek(
                request->file, request->process, request->inode.size, VFS_SEEK_SET,
                (VfsFunctionCallbackSizeT)minixOpenSeekCallback, request
            );
        } else if ((request->flags & VFS_OPEN_TRUNC) != 0) {
            file->base.functions->trunc(
                (VfsFile*)file, request->process, 0,
                (VfsFunctionCallbackVoid)minixOpenTruncCallback, request
            );
        } else {
            unlockTaskLock(&request->fs->lock);
            request->callback(simpleError(SUCCESS), request->file, request->udata);
            dealloc(request);
        }
    }
}

static void minixOpenCreateDirCloseCallback(Error error, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockTaskLock(&request->fs->lock);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        vfsReadAt(
            request->fs->block_device, NULL, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, request->inodenum),
            (VfsFunctionCallbackSizeT)minixOpenReadCallback, request
        );
    }
}

static void minixOpenCreateWriteDirCallback(Error error, size_t read, MinixOpenRequest* request) {
    if (isError(error)) {
        unlockTaskLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, NULL, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        request->file->functions->close(
            (VfsFile*)request->file, NULL, (VfsFunctionCallbackVoid)minixOpenCreateDirCloseCallback, request
        );
    }
}

static void minixOpenWriteCallback(Error error, size_t read, MinixOpenRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockTaskLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, NULL, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else if (read != sizeof(MinixInode)) {
        dealloc(request->path);
        unlockTaskLock(&request->fs->lock);
        request->error = simpleError(EIO);
        request->file->functions->close(
            (VfsFile*)request->file, NULL, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
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
            request->file, request->process, virtPtrForKernel(&request->entry),
            sizeof(MinixDirEntry), (VfsFunctionCallbackSizeT)minixOpenCreateWriteDirCallback,
            request
        );
    }
}

static void minixOpenGetINodeCallback(Error error, uint32_t inode, MinixOpenRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        unlockTaskLock(&request->fs->lock);
        request->error = error;
        request->file->functions->close(
            (VfsFile*)request->file, request->process, (VfsFunctionCallbackVoid)minixOpenCloseCallback, request
        );
    } else {
        request->inodenum = inode;
        request->inode.atime = getUnixTime();
        request->inode.ctime = getUnixTime();
        request->inode.mtime = getUnixTime();
        request->inode.gid = request->process != NULL ? request->process->resources.gid : 0;
        request->inode.mode =
            (request->mode & ~VFS_MODE_TYPE)
            | ((request->flags & VFS_OPEN_DIRECTORY) != 0 ? TYPE_MODE(VFS_TYPE_DIR) : TYPE_MODE(VFS_TYPE_REG));
        request->inode.nlinks = 1;
        request->inode.size = 0;
        request->inode.uid = request->process != NULL ? request->process->resources.uid : 0;
        memset(request->inode.zones, 0, sizeof(request->inode.zones));
        vfsWriteAt(
            request->fs->block_device, NULL, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, inode), (VfsFunctionCallbackSizeT)minixOpenWriteCallback,
            request
        );
    }
}

static void minixOpenParentINodeCallback(Error error, VfsFile* file, MinixOpenRequest* request) {
    if (isError(error)) {
        dealloc(request->path);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        lockTaskLock(&request->fs->lock);
        request->file = file;
        getFreeMinixInode(request->fs, (MinixINodeCallback)minixOpenGetINodeCallback, request);
    }
}

static void minixOpenINodeCallback(Error error, uint32_t inode, MinixOpenRequest* request) {
    if (request->path[1] != 0 && error.kind == ENOENT && (request->flags & VFS_OPEN_CREATE) != 0) {
        // If we don't find the node itself, try opening the parent
        char* path = stringClone(request->path);
        size_t len = strlen(path) - 1;
        while (len > 0 && path[len] != '/') {
            len--;
        }
        if (len == 0) {
            len++;
        }
        path[len] = 0;
        unlockTaskLock(&request->fs->lock);
        request->fs->base.functions->open(
            (VfsFilesystem*)request->fs, request->process, path,
            VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0,
            (VfsFunctionCallbackFile)minixOpenParentINodeCallback, request
        );
        dealloc(path);
    } else if (isError(error)) {
        dealloc(request->path);
        unlockTaskLock(&request->fs->lock);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else if ((request->flags & VFS_OPEN_EXCL) != 0) {
        // The file should not exist
        dealloc(request->path);
        unlockTaskLock(&request->fs->lock);
        request->callback(simpleError(EEXIST), NULL, request->udata);
        dealloc(request);
    } else {
        dealloc(request->path);
        request->inodenum = inode;
        vfsReadAt(
            request->fs->block_device, NULL, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, inode), (VfsFunctionCallbackSizeT)minixOpenReadCallback,
            request
        );
    }
}

static void minixOpenFunction(
    MinixFilesystem* fs, Process* process, const char* path, VfsOpenFlags flags, VfsMode mode,
    VfsFunctionCallbackFile callback, void* udata
) {
    lockTaskLock(&fs->lock);
    uint32_t inodenum;
    Error err = minixFindINodeForPath(fs, process, path, &inodenum);
    if (path[1] != 0 && err.kind == ENOENT && (flags & VFS_OPEN_CREATE) != 0) {
        // If we don't find the node itself, try opening the parent
        char* parent_path = getParentPath(parent_path);
        VfsFile* parent;
        CHECKED(fs->base.functions->open(
            (VfsFilesystem*)fs, process, path, VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0, &parent
        ), {
            unlockTaskLock(&request->fs->lock);
        });
        dealloc(parent_path);
        unlockTaskLock(&request->fs->lock);
        return simpleError(SUCCESS);
    } else if (isError(err)) {
        unlockTaskLock(&request->fs->lock);
        return err;
    } else if ((request->flags & VFS_OPEN_EXCL) != 0) {
        // The file should not exist
        unlockTaskLock(&fs->lock);
        return simpleError(EEXIST);
    } else {
        dealloc(request->path);
        request->inodenum = inode;
        vfsReadAt(
            request->fs->block_device, NULL, virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, inode), (VfsFunctionCallbackSizeT)minixOpenReadCallback,
            request
        );
    }
}

static Error minixAppendDirEntryInto(VfsFile* file, Process* process, uint32_t inodenum, const char* name) {
    MinixDirEntry entry = { .inode = inodenum };
    size_t name_len = strlen(name) + 1;
    if (name_len > sizeof(entry.name)) {
        name_len = sizeof(entry.name);
    }
    memcpy(entry.name, name, name_len);
    size_t tmp_size;
    CHECKED(file->functions->write(file, process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), &tmp_size));
    if (tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EIO);
    }
    return simpleError(SUCCESS);
}

static Error minixCreateNewInode(MinixFilesystem* fs, Process* process, VfsMode mode, uint32_t* inodenum) {
    CHECKED(getFreeMinixInode(fs, inodenum));
    MinixInode inode = {
        .atime = getUnixTime();
        .ctime = getUnixTime();
        .mtime = getUnixTime();
        .gid = process != NULL ? process->resources.gid : 0;
        .mode = mode;
        .nlinks = 1;
        .size = 0;
        .uid = process != NULL ? process->resources.uid : 0,
    };
    memset(inode.zones, 0, sizeof(inode.zones));
    size_t tmp_size;
    CHECKED(vfsWriteAt(
        fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, *inodenum), &tmp_size
    ));
    if (tmp_size != sizeof(MinixInode)) {
        return simpleError(EIO);
    }
    return simpleError(SUCCESS);
}

static void minixMknodFunction(
    MinixFilesystem* fs, Process* process, const char* path, VfsMode mode, DeviceId dev, VfsFunctionCallbackVoid callback, void* udata
) {
    lockTaskLock(&fs->lock);
    uint32_t inodenum;
    Error err = minixFindINodeForPath(fs, process, path, &inodenum);
    if (err.kind == ENOENT) {
        // If we don't find the node itself, try opening the parent
        char* parent_path = getParentPath(path);
        VfsFile* parent;
        CHECKED(fs->base.functions->open(
            (VfsFilesystem*)fs, process, path, VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0, &parent
        ), {
            dealloc(parent_path);
            unlockTaskLock(&fs->lock);
        });
        dealloc(parent_path);
        CHECKED(minixCreateNewInode(fs, process, mode, &inodenum), {
            parent->functions->close(parent, NULL);
            unlockTaskLock(&fs->lock);
        });
        CHECKED(minixAppendDirEntryInto(fs, process, mode, inodenum, getBaseFilename(path)), {
            parent->functions->close(parent, NULL);
            unlockTaskLock(&fs->lock);
        });
        parent->functions->close(parent, NULL);
        unlockTaskLock(&fs->lock);
        return simpleError(SUCCESS);
    } else if (isError(err)) {
        unlockTaskLock(&fs->lock);
        return err;
    } else {
        unlockTaskLock(&fs->lock);
        return simpleError(EEXIST);
    }
}

static Error minixCheckDirectoryCanBeRemoved(VfsFile* file, Process* process, MinixInode* inode) {
    if (inode->size < sizeof(MinixDirEntry)) {
        return simpleError(SUCCESS);
    } else if (inode->size > 2 * sizeof(MinixDirEntry)) {
        return simpleError(EPERM);
    } else {
        size_t tmp_size;
        MinixDirEntry entries[2];
        CHECKED(file->functions->read(file, process, virtPtrForKernel(entries), inode.size, &tmp_size));
        if (tmp_size != inode.size) {
            return simpleError(EIO);
        } else if (strcmp((char*)entries[0].name, ".") != 0 && strcmp((char*)entries[0].name, "..") != 0) {
            return simpleError(EPERM);
        } else if (
            inode->size == 2 * sizeof(MinixDirEntry)
            && strcmp((char*)entries[1].name, ".") != 0
            && strcmp((char*)entries[1].name, "..") != 0
        ) {
            return simpleError(EPERM);
        } else {
            return simpleError(SUCCESS);
        }
    }
}

static Error minixChangeInodeRefCount(MinixFilesystem* fs, Process* process, uint32_t inodenum, int delta) {
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, inodenum), &tmp_size));
    if (tmp_size != sizeof(MinixInode)) {
        return simpleError(EIO);
    }
    inode.nlinks += delta;
    if (inode.nlinks == 0) {
        VfsFile* file = (VfsFile*)createMinixFileForINode(fs, inodenum, true);
        if (MODE_TYPE(stat.mode) != VFS_TYPE_DIR) {
            CHECKED(minixCheckDirectoryCanBeRemoved(file, process, &inode), file->functions->close(file, NULL));
        }
        CHECKED(file->functions->trunc(file, process, 0), file->functions->close(file, NULL));
        file->functions->close(file, NULL);
        CHECKED(freeMinixInode(fs, inodenum));
        return simpleError(SUCCESS);
    } else {
        CHECKED(vfsWriteAt(fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, old_inode), &tmp_size));
        if (tmp_size != sizeof(MinixInode)) {
            return simpleError(EIO);
        }
        return simpleError(SUCCESS);
    }
}

static Error minixRemoveDirectoryEntry(MinixFilesystem* fs, Process* process, VfsFile* parent, const char* name) {
    uint32_t inodenum;
    size_t offset;
    size_t dir_size;
    CHECKED(minixFindINodeForNameIn((MinixFile*)parent, process, name, &inodenum, &offset, &dir_size));
    CHECKED(minixChangeInodeRefCount(fs, inodenum, -1))
    size_t tmp_size;
    MinixDirEntry entry;
    CHECKED(vfsReadAt(parent, request->process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), dir_size - sizeof(MinixDirEntry), &tmp_size));
    if (tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EIO);
    }
    CHECKED(vfsWriteAt(parent, process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), offset, &tmp_size));
    if (tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EIO);
    }
    CHECKED(parent->functions->trunc(parent, process, dir_size - sizeof(MinixDirEntry)));
    return simpleError(SUCCESS);
}

static Error minixUnlinkFunction(MinixFilesystem* fs, Process* process, const char* path) {
    lockTaskLock(&fs->lock);
    // Try opening the parent
    char* parent_path = getParentPath(path);
    VfsFile* parent;
    CHECKED(fs->base.functions->open((VfsFilesystem*)fs, process, path_copy, VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0, &parent), {
        dealloc(parent_path);
        unlockTaskLock(&fs->lock);
    });
    dealloc(parent_path);
    CHECKED(minixRemoveDirectoryEntry(fs, process, parent, getBaseFilename(name)), {
        parent->functions->close(parent, NULL);
        unlockTaskLock(&request->fs->lock);
    });
    parent->functions->close(parent, NULL);
    unlockTaskLock(&request->fs->lock);
    return simpleError(SUCCESS);
}

static Error minixLinkFunction(MinixFilesystem* fs, Process* process, const char* old, const char* new) {
    lockTaskLock(&fs->lock);
    uint32_t inodenum;
    Error err = minixFindINodeForPath(fs, process, new, &inodenum);
    if (error.kind == ENOENT) {
        CHECKED(minixFindINodeForPath(fs, process, old, &inodenum), unlockTaskLock(&fs->lock));
        // Try opening the parent
        char* parent_path = getParentPath(new);
        VfsFile* parent;
        CHECKED(fs->base.functions->open(
            (VfsFilesystem*)fs, process, parent_path, VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0, &parent
        ), {
            dealloc(parent_path);
            unlockTaskLock(&fs->lock);
        });
        dealloc(parent_path);
        CHECKED(minixChangeInodeRefCount(fs, process, inodenum, 1), {
            parent->functions->close(parent, NULL);
            unlockTaskLock(&fs->lock);
        });
        CHECKED(minixAppendDirEntryInto(parent, process, inodenum, getBaseFilename(new)), {
            parent->functions->close(parent, NULL);
            unlockTaskLock(&fs->lock);
        });
        parent->functions->close(parent, NULL);
        unlockTaskLock(&fs->lock);
        return simpleError(SUCCESS);
    } else {
        unlockTaskLock(&fs->lock);
        if (isError(err)) {
            return err;
        } else {
            return simpleError(EEXIST);
        }
    }
}

static Error minixRenameFunction(MinixFilesystem* fs, Process* process, const char* old, const char* new) {
    // Implementation is not ideal, but simple
    CHECKED(minixLinkFunction(fs, process, old, new));
    CHECKED(minixUnlinkFunction(fs, process, old));
    return simpleError(SUCCESS);
}

static void minixFreeFunction(MinixFilesystem* fs, Process* process) {
    // We don't do any caching. Otherwise write it to disk here.
    fs->block_device->functions->close(fs->block_device, NULL);
    dealloc(fs);
}

static Error minixInitFunction(MinixFilesystem* fs, Process* process) {
    size_t tmp_size;
    CHECKED(vfsReadAt(
        fs->block_device, NULL, virtPtrForKernel(&fs->superblock), sizeof(Minix3Superblock),
        MINIX_BLOCK_SIZE, &tmp_size
    ));
    if (tmp_size != sizeof(Minix3Superblock)) {
        return simpleError(EIO);
    } else if (fs->superblock.magic != MINIX3_MAGIC) {
        return simpleError(EPERM);
    } else {
        // Don't do more for now
        return simpleError(SUCCESS);
    }
}

static const VfsFilesystemVtable functions = {
    .open = (OpenFunction)minixOpenFunction,
    .mknod = (MknodFunction)minixMknodFunction,
    .unlink = (UnlinkFunction)minixUnlinkFunction,
    .link = (LinkFunction)minixLinkFunction,
    .rename = (RenameFunction)minixRenameFunction,
    .free = (FreeFunction)minixFreeFunction,
    .init = (InitFunction)minixInitFunction,
};

MinixFilesystem* createMinixFilesystem(VfsFile* block_device, VirtPtr data) {
    MinixFilesystem* file = zalloc(sizeof(MinixFilesystem));
    file->base.functions = &functions;
    file->block_device = block_device;
    return file;
}

