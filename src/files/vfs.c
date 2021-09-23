
#include <assert.h>
#include <string.h>

#include "error/error.h"
#include "files/vfs.h"

#include "files/path.h"
#include "memory/kalloc.h"

VirtualFilesystem global_file_system;

Error initVirtualFileSystem() {
    return simpleError(SUCCESS);
}

VirtualFilesystem* createVirtualFilesystem() {
    return zalloc(sizeof(VirtualFilesystem));
}

static void freeFilesystem(Error error, void* to_free) {
    dealloc(to_free);
}

static bool freeFilesystemMount(FilesystemMount* mount, bool force) {
    dealloc(mount->path);
    switch (mount->type) {
        case MOUNT_TYPE_FILE: {
            VfsFile* file = (VfsFile*)mount->data;
            file->functions->close(file, 0, 0, freeFilesystem, file);
            return true;
        }
        case MOUNT_TYPE_FS: {
            VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
            if (!force && filesystem->open_files != 0) {
                return false;
            } else {
                filesystem->functions->free(filesystem, 0, 0, freeFilesystem, filesystem);
                return true;
            }
        }
        case MOUNT_TYPE_BIND:
            dealloc(mount->data);
            return true;
    }
}

void freeVirtualFilesystem(VirtualFilesystem* fs) {
    lockSpinLock(&fs->lock);
    for (size_t i = 0; i < fs->mount_count; i++) {
        freeFilesystemMount(&fs->mounts[i], true);
    }
    dealloc(fs->mounts);
    dealloc(fs);
}

Error mountFilesystem(VirtualFilesystem* fs, VfsFilesystem* filesystem, const char* path) {
    assert(path[0] == '/');
    lockSpinLock(&fs->lock);
    fs->mounts = krealloc(fs->mounts, (fs->mount_count + 1) * sizeof(FilesystemMount));
    fs->mounts[fs->mount_count].type = MOUNT_TYPE_FS;
    fs->mounts[fs->mount_count].path = reducedPathCopy(path);
    fs->mounts[fs->mount_count].data = filesystem;
    fs->mount_count++;
    unlockSpinLock(&fs->lock);
    return simpleError(SUCCESS);
}

Error mountFile(VirtualFilesystem* fs, VfsFile* file, const char* path) {
    assert(path[0] == '/');
    lockSpinLock(&fs->lock);
    fs->mounts = krealloc(fs->mounts, (fs->mount_count + 1) * sizeof(FilesystemMount));
    fs->mounts[fs->mount_count].type = MOUNT_TYPE_FILE;
    fs->mounts[fs->mount_count].path = reducedPathCopy(path);
    fs->mounts[fs->mount_count].data = file;
    fs->mount_count++;
    unlockSpinLock(&fs->lock);
    return simpleError(SUCCESS);
}

Error mountRedirect(VirtualFilesystem* fs, const char* from, const char* to) {
    assert(from[0] == '/' && to[0] == '/');
    lockSpinLock(&fs->lock);
    fs->mounts = krealloc(fs->mounts, (fs->mount_count + 1) * sizeof(FilesystemMount));
    fs->mounts[fs->mount_count].type = MOUNT_TYPE_BIND;
    fs->mounts[fs->mount_count].path = reducedPathCopy(to);
    fs->mounts[fs->mount_count].data = reducedPathCopy(from);
    fs->mount_count++;
    unlockSpinLock(&fs->lock);
    return simpleError(SUCCESS);
}

Error umount(VirtualFilesystem* fs, const char* from) {
    lockSpinLock(&fs->lock);
    for (size_t i = 0; i < fs->mount_count;) {
        if (strcmp(fs->mounts[i].path, from) == 0) {
            if (freeFilesystemMount(&fs->mounts[i], false)) {
                fs->mount_count--;
                memmove(fs->mounts + i, fs->mounts + i + 1, fs->mount_count - i);
                unlockSpinLock(&fs->lock);
                return simpleError(SUCCESS);
            } else {
                unlockSpinLock(&fs->lock);
                return simpleError(ALREADY_IN_USE);
            }
        } else {
            i++;
        }
    }
    if (fs->parent != NULL) {
        CHECKED(umount(fs->parent, from), unlockSpinLock(&fs->lock));
    }
    unlockSpinLock(&fs->lock);
    return simpleError(SUCCESS);
}

static FilesystemMount* findMountHandling(VirtualFilesystem* fs, char* path) {
    for (size_t i = fs->mount_count; i != 0;) {
        i--;
        const char* mount_path = fs->mounts[i].path;
        size_t mount_path_length = strlen(mount_path);
        if (
            strncmp(mount_path, path, mount_path_length) == 0
            && (
                path[mount_path_length] == 0 || path[mount_path_length] == '/'
                || mount_path_length == 1
            )
        ) {
            switch (fs->mounts[i].type) {
                case MOUNT_TYPE_FILE:
                    if (path[mount_path_length] == 0) { // Only an exact match is possible here
                        return &fs->mounts[i];
                    }
                case MOUNT_TYPE_FS:
                    return &fs->mounts[i];
                case MOUNT_TYPE_BIND: {
                    const char* new_path = (const char*)fs->mounts[i].data;
                    size_t new_path_length = strlen(new_path);
                    size_t path_length = strlen(path) - mount_path_length;
                    char* compined = kalloc(new_path_length + path_length + 2);
                    memcpy(compined, new_path, new_path_length);
                    compined[new_path_length] = '/';
                    memcpy(compined + new_path_length + 1, path + mount_path_length, path_length);
                    compined[new_path_length + path_length + 1] = '/';
                    dealloc(path);
                    path = compined;
                    inlineReducePath(path);
                    // Continue searching for the complete path
                    break;
                }
            }
        }
    }
    dealloc(path);
    return NULL;
}

void vfsOpen(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, reducedPathCopy(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                VfsFile* file = (VfsFile*)mount->data;
                unlockSpinLock(&fs->lock);
                file->functions->dup(file, uid, gid, callback, udata);
                return;
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                const char* moved = path;
                if (mount->path[1] != 0) {
                    // If the mount is not to '/' remove the prefix.
                    // e.g. if the mount is to '/mnt' remove '/mnt' from '/mnt/test'
                    moved += strlen(mount->path);
                }
                unlockSpinLock(&fs->lock);
                filesystem->functions->open(filesystem, uid, gid, moved, flags, mode, callback, udata);
                return;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else if (fs->parent != NULL) {
        VirtualFilesystem* parent = fs->parent;
        unlockSpinLock(&fs->lock);
        vfsOpen(parent, uid, gid, path, flags, mode, callback, udata);
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(NO_SUCH_FILE), NULL, udata);
    }
}

void vfsUnlink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, reducedPathCopy(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                unlockSpinLock(&fs->lock);
                callback(simpleError(UNSUPPORTED), udata);
                break;
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                unlockSpinLock(&fs->lock);
                filesystem->functions->unlink(filesystem, uid, gid, path, callback, udata);
                break;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(NO_SUCH_FILE), udata);
    }
}

void vfsLink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, reducedPathCopy(old));
    FilesystemMount* mount_new = findMountHandling(fs, reducedPathCopy(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Linking across filesystem boundaries is impossible
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            unlockSpinLock(&fs->lock);
            filesystem->functions->link(filesystem, uid, gid, old, new, callback, udata);
        } else {
            unlockSpinLock(&fs->lock);
            callback(simpleError(UNSUPPORTED), udata);
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(NO_SUCH_FILE), udata);
    }
}

void vfsRename(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    // Could just be a link and unlink.
    lockSpinLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, reducedPathCopy(old));
    FilesystemMount* mount_new = findMountHandling(fs, reducedPathCopy(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Moving across filesystem boundaries requires copying
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            unlockSpinLock(&fs->lock);
            filesystem->functions->rename(filesystem, uid, gid, old, new, callback, udata);
        } else {
            unlockSpinLock(&fs->lock);
            callback(simpleError(UNSUPPORTED), udata);
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(NO_SUCH_FILE), udata);
    }
}

typedef struct {
    VfsFile* file;
    Uid uid;
    Gid gid;
    VirtPtr buffer;
    size_t offset;
    size_t size;
    bool write;
    VfsFunctionCallbackSizeT callback;
    void* udata;
} VfsReadAtRequest;

static void vfsOperationAtReadWriteCallback(Error error, size_t read, VfsReadAtRequest* request) {
    if (isError(error)) {
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (read == 0 || request->size == 0) {
        request->callback(simpleError(SUCCESS), request->offset, request->udata);
        dealloc(request);
    } else {
        request->size -= read;
        request->offset += read;
        request->buffer.address += read;
        if (request->size != 0) {
            // Try to read/write the remaining bytes
            if (request->write) {
                request->file->functions->write(
                    request->file, request->uid, request->gid, request->buffer, request->size,
                    (VfsFunctionCallbackSizeT)vfsOperationAtReadWriteCallback, request
                );
            } else {
                request->file->functions->read(
                    request->file, request->uid, request->gid, request->buffer, request->size,
                    (VfsFunctionCallbackSizeT)vfsOperationAtReadWriteCallback, request
                );
            }
        } else {
            // Don't read more for now
            request->callback(simpleError(SUCCESS), request->offset, request->udata);
            dealloc(request);
        }
    }
}

static void vfsOperationAtSeekCallback(Error error, size_t offset, VfsReadAtRequest* request) {
    if (isError(error)) {
        request->callback(error, 0, request->udata);
        dealloc(request);
    } else if (offset != request->offset) {
        request->callback(simpleError(IO_ERROR), 0, request->udata);
        dealloc(request);
    } else {
        request->offset = 0;
        if (request->write) {
            request->file->functions->write(
                request->file, request->uid, request->gid, request->buffer, request->size,
                (VfsFunctionCallbackSizeT)vfsOperationAtReadWriteCallback, request
            );
        } else {
            request->file->functions->read(
                request->file, request->uid, request->gid, request->buffer, request->size,
                (VfsFunctionCallbackSizeT)vfsOperationAtReadWriteCallback, request
            );
        }
    }
}

static void vfsGenericOperationAt(
    VfsFile* file, Uid uid, Gid gid, VirtPtr ptr, size_t size, size_t offset,
    bool write, VfsFunctionCallbackSizeT callback, void* udata
) {
    VfsReadAtRequest* request = kalloc(sizeof(VfsReadAtRequest));
    request->file = file;
    request->uid = uid;
    request->gid = gid;
    request->buffer = ptr;
    request->offset = offset;
    request->size = size;
    request->write = write;
    request->callback = callback;
    request->udata = udata;
    file->functions->seek(
        file, request->uid, request->gid, offset, VFS_SEEK_SET,
        (VfsFunctionCallbackSizeT)vfsOperationAtSeekCallback, request
    );
}

void vfsReadAt(VfsFile* file, Uid uid, Gid gid, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata) {
    vfsGenericOperationAt(file, uid, gid, ptr, size, offset, false, callback, udata);
}

void vfsWriteAt(VfsFile* file, Uid uid, Gid gid, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata) {
    vfsGenericOperationAt(file, uid, gid, ptr, size, offset, true, callback, udata);
}

bool canAccess(VfsMode mode, Uid file_uid, Gid file_gid, Uid uid, Gid gid, VfsAccessFlags flags) {
    if (uid == 0 || gid == 0) {
        // User and group id 0 are allowed to do everything
        return true;
    } else if (
        (flags & VFS_ACCESS_R) != 0 && (mode & VFS_MODE_A_R) == 0
        && ((mode & VFS_MODE_G_R) == 0 || file_gid != gid)
        && ((mode & VFS_MODE_O_R) == 0 || file_uid != uid)
    ) {
        return false;
    } else if (
        (flags & VFS_ACCESS_W) != 0 && (mode & VFS_MODE_A_W) == 0
        && ((mode & VFS_MODE_G_W) == 0 || file_gid != gid)
        && ((mode & VFS_MODE_O_W) == 0 || file_uid != uid)
    ) {
        return false;
    } else if (
        (flags & VFS_ACCESS_X) != 0 && (mode & VFS_MODE_A_X) == 0
        && ((mode & VFS_MODE_G_X) == 0 || file_gid != gid)
        && ((mode & VFS_MODE_O_X) == 0 || file_uid != uid)
    ) {
        return false;
    } else if ((flags & VFS_ACCESS_REG) != 0 && MODE_TYPE(mode) != VFS_TYPE_REG) {
        return false;
    } else if ((flags & VFS_ACCESS_DIR) != 0 && MODE_TYPE(mode) != VFS_TYPE_DIR) {
        return false;
    } else {
        return true;
    }
}

