
#include <stddef.h>
#include <string.h>

#include "error/log.h"
#include "files/minix/minix.h"
#include "files/minix/file.h"

#include "files/path.h"
#include "files/vfs.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode) {
    return (2 + fs->superblock.imap_blocks + fs->superblock.zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

typedef void (*MinixINodeCallback)(Error error, uint32_t inode, void* udata);

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
        request->file->base.functions->read(
            (VfsFile*)request->file, request->uid, request->gid, virtPtrForKernel(request->entries),
            size, (VfsFunctionCallbackSizeT)minixFindINodeInReadCallback, request
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
            if (strcmp((char*)request->entries[i].name, request->name) == 0) {
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
    file->base.functions->stat((VfsFile*)file, uid, gid, (VfsFunctionCallbackStat)minixFindINodeInStatCallback, request);
}

typedef struct {
    const MinixFilesystem* fs;
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
            (VfsFile*)request->file, request->uid, request->gid,
            (VfsFunctionCallbackVoid)minixFindINodeCloseCallback, request
        );
    } else {
        minixFindINodeCloseCallback(simpleError(SUCCESS), request);
    }
}

static void minixFindINodeForPath(const MinixFilesystem* fs, Uid uid, Gid gid, const char* path, MinixINodeCallback callback, void* udata) {
    MinixFindINodeRequest* request = kalloc(sizeof(MinixFindINodeRequest));
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->path_copy = reducedPathCopy(path);
    request->path = request->path_copy + 1; // all paths start with /, skip it
    request->callback = callback;
    request->udata = udata;
    minixFindINodeStepCallback(simpleError(SUCCESS), 1, request); // Start searching at the root node
}

typedef struct {
    const MinixFilesystem* fs;
    Uid uid;
    Gid gid;
    VfsOpenFlags flags;
    VfsMode mode;
    VfsFunctionCallbackFile callback;
    void* udata;
    VfsFile* file;
    uint32_t inodenum;
    MinixInode inode;
} MinixOpenRequest;

static void minixOpenSeekCallback(Error error, size_t pos, MinixOpenRequest* request) {
    if (isError(error)) {
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        request->callback(simpleError(SUCCESS), request->file, request->udata);
        dealloc(request);
    }
}

static void minixOpenTruncCallback(Error error, MinixOpenRequest* request) {
    if (isError(error)) {
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        request->callback(simpleError(SUCCESS), request->file, request->udata);
        dealloc(request);
    }
}

static void minixOpenReadCallback(Error error, size_t read, MinixOpenRequest* request) {
    if (isError(error)) {
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else if (read != sizeof(MinixInode)) {
        request->callback(simpleError(IO_ERROR), NULL, request->udata);
        dealloc(request);
    } else
        // TODO: check permissions
        if (MODE_TYPE(request->inode.mode) != VFS_TYPE_DIR && (request->flags & VFS_OPEN_DIRECTORY) != 0) {
        request->callback(simpleError(WRONG_FILE_TYPE), NULL, request->udata);
        dealloc(request);
    } else if (MODE_TYPE(request->inode.mode) == VFS_TYPE_DIR && (request->flags & VFS_OPEN_DIRECTORY) == 0) {
        request->callback(simpleError(WRONG_FILE_TYPE), NULL, request->udata);
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
            request->callback(simpleError(SUCCESS), request->file, request->udata);
            dealloc(request);
        }
    }
}

static void minixOpenINodeCallback(Error error, uint32_t inode, MinixOpenRequest* request) {
    if (error.kind == NO_SUCH_FILE && (request->flags & VFS_OPEN_CREATE) != 0) {
        // TODO!
    } else if (isError(error)) {
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        request->inodenum = inode;
        vfsReadAt(
            request->fs->block_device, request->uid, request->gid,
            virtPtrForKernel(&request->inode), sizeof(MinixInode),
            offsetForINode(request->fs, inode), (VfsFunctionCallbackSizeT)minixOpenReadCallback,
            request
        );
    }
}

static void minixOpenFunction(
    const MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode,
    VfsFunctionCallbackFile callback, void* udata
) {
    MinixOpenRequest* request = kalloc(sizeof(MinixOpenRequest));
    request->fs = fs;
    request->uid = uid;
    request->gid = gid;
    request->flags = flags;
    request->mode = mode;
    request->callback = callback;
    request->udata = udata;
    minixFindINodeForPath(fs, uid, gid, path, (MinixINodeCallback)minixOpenINodeCallback, request);
}

static void minixUnlinkFunction(const MinixFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
    // TODO: implement
}

static void minixLinkFunction(const MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    // TODO: implement
}

typedef struct {
    const MinixFilesystem* fs;
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

static void minixRenameFunction(const MinixFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
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
    Uid uid;
    Gid gid;
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
    request->uid = uid;
    request->gid = gid;
    request->callback = callback;
    request->udata = udata;
    vfsReadAt(
        fs->block_device, uid, gid, virtPtrForKernel(&fs->superblock), sizeof(Minix3Superblock),
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

