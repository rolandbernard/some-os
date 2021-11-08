
#include <assert.h>
#include <string.h>

#include "files/vfs.h"

#include "files/special/fifo.h"
#include "files/minix/minix.h"
#include "devices/devices.h"
#include "memory/kalloc.h"
#include "error/error.h"
#include "files/path.h"
#include "util/util.h"

VirtualFilesystem global_file_system;

Error initVirtualFileSystem() {
    CHECKED(mountDeviceFiles());
    return simpleError(SUCCESS);
}

VirtualFilesystem* createVirtualFilesystem() {
    return zalloc(sizeof(VirtualFilesystem));
}

static bool freeFilesystemMount(FilesystemMount* mount, bool force) {
    dealloc(mount->path);
    switch (mount->type) {
        case MOUNT_TYPE_FILE: {
            VfsFile* file = (VfsFile*)mount->data;
            file->functions->close(file, NULL, noop, NULL);
            return true;
        }
        case MOUNT_TYPE_FS: {
            VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
            if (!force && filesystem->open_files != 0) {
                return false;
            } else {
                filesystem->functions->free(filesystem, NULL, noop, NULL);
                return true;
            }
        }
        case MOUNT_TYPE_BIND:
            dealloc(mount->data);
            return true;
        default: 
            panic();
            return false;
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
    fs->mounts[fs->mount_count].path = stringClone(path);
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
    fs->mounts[fs->mount_count].path = stringClone(path);
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
    fs->mounts[fs->mount_count].path = stringClone(to);
    fs->mounts[fs->mount_count].data = stringClone(from);
    fs->mount_count++;
    unlockSpinLock(&fs->lock);
    return simpleError(SUCCESS);
}

Error umount(VirtualFilesystem* fs, const char* from) {
    lockSpinLock(&fs->lock);
    for (size_t i = fs->mount_count; i > 0;) {
        i--;
        if (strcmp(fs->mounts[i].path, from) == 0) {
            if (freeFilesystemMount(&fs->mounts[i], false)) {
                fs->mount_count--;
                memmove(fs->mounts + i, fs->mounts + i + 1, fs->mount_count - i);
                unlockSpinLock(&fs->lock);
                return simpleError(SUCCESS);
            } else {
                unlockSpinLock(&fs->lock);
                return simpleError(EBUSY);
            }
        }
    }
    if (fs->parent != NULL) {
        CHECKED(umount(fs->parent, from), unlockSpinLock(&fs->lock));
        unlockSpinLock(&fs->lock);
        return simpleError(SUCCESS);
    } else {
        unlockSpinLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

static FilesystemMount* findLocalMountHandling(VirtualFilesystem* fs, const char* path) {
    FilesystemMount* ret = NULL;
    if (fs->parent != NULL) {
        ret = findLocalMountHandling(fs->parent, path);
    }
    for (size_t i = 0; i < fs->mount_count; i++) {
        const char* mount_path = fs->mounts[i].path;
        size_t mount_path_length = strlen(mount_path);
        if (
            (ret == NULL || strlen(ret->path) < mount_path_length) // Find the longest matching mount
            && strncmp(mount_path, path, mount_path_length) == 0
            && (
                path[mount_path_length] == 0 || path[mount_path_length] == '/'
                || mount_path_length == 1
            )
        ) {
            switch (fs->mounts[i].type) {
                case MOUNT_TYPE_FILE:
                    if (path[mount_path_length] == 0) { // Only an exact match is possible here
                        ret = &fs->mounts[i];
                    }
                    break;
                case MOUNT_TYPE_FS:
                case MOUNT_TYPE_BIND: {
                    ret = &fs->mounts[i];
                    break;
                }
            }
        }
    }
    return ret;
}

static FilesystemMount* findMountHandling(VirtualFilesystem* fs, char* path) {
    FilesystemMount* mount = NULL;
    do {
        mount = findLocalMountHandling(fs, path);
        if (mount != NULL && mount->type == MOUNT_TYPE_BIND) {
            size_t mount_path_length = strlen(mount->path);
            const char* new_path = (const char*)mount->data;
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
        }
    } while (mount != NULL && mount->type == MOUNT_TYPE_BIND);
    dealloc(path);
    return mount;
}

static char* toMountPath(FilesystemMount* mount, const char* path) {
    size_t mount_length = strlen(mount->path);
    size_t path_length = strlen(path);
    char* path_copy = stringClone(path);
    if (mount_length != 1) {
        // If the mount is not to '/' remove the prefix.
        if (mount_length == path_length) {
            // If the mount point is equal to the requested path,
            // the result is '/'
            memcpy(path_copy, "/", 2);
        } else {
            // e.g. if the mount is to '/mnt' remove '/mnt' from '/mnt/test'
            memmove(path_copy, path_copy + mount_length, path_length - mount_length + 1);
        }
    }
    return path_copy;
}

typedef struct {
    char* path;
    VfsFunctionCallbackFile callback;
    void* udata;
} VfsOpenRequest;

static void postOpenOperations(Error error, VfsFile* file, void* udata) {
    VfsOpenRequest* request = (VfsOpenRequest*)udata;
    if (isError(error)) {
        request->callback(error, file, request->udata);
    } else {
        if (MODE_TYPE(file->mode) == VFS_TYPE_FIFO) {
            // This is a fifo, we have to create one
            FifoFile* fifo = createFifoFile(request->path, file->mode, file->uid, file->gid);
            file->functions->close(file, NULL, noop, NULL);
            if (fifo != NULL) {
                request->callback(simpleError(SUCCESS), (VfsFile*)fifo, request->udata);
            } else {
                request->callback(simpleError(ENOMEM), NULL, request->udata);
            }
        } else {
            request->callback(simpleError(SUCCESS), file, request->udata);
        }
    }
    dealloc(request->path);
    dealloc(request);
}

static VfsOpenRequest* createOpenRequest(const char* path, VfsFunctionCallbackFile callback, void* udata) {
    VfsOpenRequest* ret = kalloc(sizeof(VfsOpenRequest));
    ret->path = stringClone(path);
    ret->callback = callback;
    ret->udata = udata;
    return ret;
}

void vfsOpen(VirtualFilesystem* fs, struct Process_s* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, stringClone(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                VfsFile* file = (VfsFile*)mount->data;
                unlockSpinLock(&fs->lock);
                file->functions->dup(file, process, postOpenOperations, createOpenRequest(path, callback, udata));
                return;
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                if (filesystem->functions->open != NULL) {
                    unlockSpinLock(&fs->lock);
                    char* path_copy = toMountPath(mount, path);
                    filesystem->functions->open(filesystem, process, path_copy, flags, mode, postOpenOperations, createOpenRequest(path, callback, udata));
                    dealloc(path_copy);
                } else {
                    unlockSpinLock(&fs->lock);
                    callback(simpleError(EPERM), NULL, udata);
                }
                return;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(ENOENT), NULL, udata);
    }
}

void vfsMknod(VirtualFilesystem* fs, struct Process_s* process, const char* path, VfsMode mode, DeviceId dev, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, stringClone(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                unlockSpinLock(&fs->lock);
                callback(simpleError(ENOENT), udata);
                return;
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                if (filesystem->functions->mknod != NULL) {
                    unlockSpinLock(&fs->lock);
                    char* path_copy = toMountPath(mount, path);
                    filesystem->functions->mknod(filesystem, process, path_copy, mode, dev, callback, udata);
                    dealloc(path_copy);
                } else {
                    unlockSpinLock(&fs->lock);
                    callback(simpleError(EPERM), udata);
                }
                return;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(ENOENT), udata);
    }
}

void vfsUnlink(VirtualFilesystem* fs, struct Process_s* process, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, stringClone(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                unlockSpinLock(&fs->lock);
                callback(simpleError(EPERM), udata);
                break;
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                unlockSpinLock(&fs->lock);
                if (filesystem->functions->unlink != NULL) {
                    char* path_copy = toMountPath(mount, path);
                    filesystem->functions->unlink(filesystem, process, path_copy, callback, udata);
                    dealloc(path_copy);
                } else {
                    callback(simpleError(EPERM), udata);
                }
                break;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(ENOENT), udata);
    }
}

void vfsLink(VirtualFilesystem* fs, struct Process_s* process, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, stringClone(old));
    FilesystemMount* mount_new = findMountHandling(fs, stringClone(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Linking across filesystem boundaries is impossible
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            unlockSpinLock(&fs->lock);
            if (filesystem->functions->link != NULL) {
                char* old_copy = toMountPath(mount_old, old);
                char* new_copy = toMountPath(mount_new, new);
                filesystem->functions->link(filesystem, process, old_copy, new_copy, callback, udata);
                dealloc(old_copy);
                dealloc(new_copy);
            } else {
                callback(simpleError(EPERM), udata);
            }
        } else {
            unlockSpinLock(&fs->lock);
            callback(simpleError(EXDEV), udata);
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(ENOENT), udata);
    }
}

void vfsRename(VirtualFilesystem* fs, struct Process_s* process, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    // Could just be a link and unlink.
    lockSpinLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, stringClone(old));
    FilesystemMount* mount_new = findMountHandling(fs, stringClone(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Moving across filesystem boundaries requires copying
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            unlockSpinLock(&fs->lock);
            if (filesystem->functions->link != NULL) {
                char* old_copy = toMountPath(mount_old, old);
                char* new_copy = toMountPath(mount_new, new);
                filesystem->functions->rename(filesystem, process, old_copy, new_copy, callback, udata);
                dealloc(old_copy);
                dealloc(new_copy);
            } else {
                callback(simpleError(EPERM), udata);
            }
        } else {
            unlockSpinLock(&fs->lock);
            callback(simpleError(EXDEV), udata);
        }
    } else {
        unlockSpinLock(&fs->lock);
        callback(simpleError(ENOENT), udata);
    }
}

typedef struct {
    VfsFile* file;
    Process* process;
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
                    request->file, request->process, request->buffer, request->size,
                    (VfsFunctionCallbackSizeT)vfsOperationAtReadWriteCallback, request
                );
            } else {
                request->file->functions->read(
                    request->file, request->process, request->buffer, request->size,
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
        request->callback(simpleError(EIO), 0, request->udata);
        dealloc(request);
    } else {
        request->offset = 0;
        if (request->write) {
            request->file->functions->write(
                request->file, request->process, request->buffer, request->size,
                (VfsFunctionCallbackSizeT)vfsOperationAtReadWriteCallback, request
            );
        } else {
            request->file->functions->read(
                request->file, request->process, request->buffer, request->size,
                (VfsFunctionCallbackSizeT)vfsOperationAtReadWriteCallback, request
            );
        }
    }
}

static void vfsGenericOperationAt(
    VfsFile* file, struct Process_s* process, VirtPtr ptr, size_t size, size_t offset,
    bool write, VfsFunctionCallbackSizeT callback, void* udata
) {
    VfsReadAtRequest* request = kalloc(sizeof(VfsReadAtRequest));
    request->file = file;
    request->process = process;
    request->buffer = ptr;
    request->offset = offset;
    request->size = size;
    request->write = write;
    request->callback = callback;
    request->udata = udata;
    file->functions->seek(
        file, process, offset, VFS_SEEK_SET,
        (VfsFunctionCallbackSizeT)vfsOperationAtSeekCallback, request
    );
}

void vfsReadAt(VfsFile* file, struct Process_s* process, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata) {
    vfsGenericOperationAt(file, process, ptr, size, offset, false, callback, udata);
}

void vfsWriteAt(VfsFile* file, struct Process_s* process, VirtPtr ptr, size_t size, size_t offset, VfsFunctionCallbackSizeT callback, void* udata) {
    vfsGenericOperationAt(file, process, ptr, size, offset, true, callback, udata);
}

bool canAccess(VfsMode mode, Uid file_uid, Gid file_gid, struct Process_s* process, VfsAccessFlags flags) {
    if (process == NULL || process->resources.uid == 0 || process->resources.gid == 0) {
        // User and group id 0 are allowed to do everything
        return true;
    } else if (
        (flags & VFS_ACCESS_R) != 0 && (mode & VFS_MODE_A_R) == 0
        && ((mode & VFS_MODE_G_R) == 0 || file_gid != process->resources.gid)
        && ((mode & VFS_MODE_O_R) == 0 || file_uid != process->resources.uid)
    ) {
        return false;
    } else if (
        (flags & VFS_ACCESS_W) != 0 && (mode & VFS_MODE_A_W) == 0
        && ((mode & VFS_MODE_G_W) == 0 || file_gid != process->resources.gid)
        && ((mode & VFS_MODE_O_W) == 0 || file_uid != process->resources.uid)
    ) {
        return false;
    } else if (
        (flags & VFS_ACCESS_X) != 0 && (mode & VFS_MODE_A_X) == 0
        && ((mode & VFS_MODE_G_X) == 0 || file_gid != process->resources.gid)
        && ((mode & VFS_MODE_O_X) == 0 || file_uid != process->resources.uid)
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

typedef struct {
    char* type;
    VirtPtr data;
    VfsFunctionCallbackFilesystem callback;
    void* udata;
    VfsFilesystem* fs;
} CreateFsRequest;

static void createFsInitCallback(Error error, CreateFsRequest* request) {
    if (isError(error)) {
        request->fs->functions->free(request->fs, NULL, noop, NULL);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        request->callback(simpleError(SUCCESS), request->fs, request->udata);
        dealloc(request);
    }
}

static void createFsOpenCallback(Error error, VfsFile* file, CreateFsRequest* request) {
    if (isError(error)) {
        dealloc(request->type);
        request->callback(error, NULL, request->udata);
        dealloc(request);
    } else {
        if (strcmp(request->type, "minix") == 0) {
            dealloc(request->type);
            MinixFilesystem* fs = createMinixFilesystem(file, request->data);
            request->fs = (VfsFilesystem*)fs;
            fs->base.functions->init(
                (VfsFilesystem*)fs, NULL, (VfsFunctionCallbackVoid)createFsInitCallback, request
            );
        } else {
            file->functions->close(file, NULL, noop, NULL);
            dealloc(request->type);
            request->callback(simpleError(EINVAL), NULL, request->udata);
            dealloc(request);
        }
    }
}

void createFilesystemFrom(VirtualFilesystem* fs, const char* path, const char* type, VirtPtr data, VfsFunctionCallbackFilesystem callback, void* udata) {
    CreateFsRequest* request = kalloc(sizeof(CreateFsRequest));
    request->data = data;
    request->type = stringClone(type);
    request->callback = callback;
    request->udata = udata;
    vfsOpen(fs, NULL, path, VFS_OPEN_READ, 0, (VfsFunctionCallbackFile)createFsOpenCallback, request);
}
