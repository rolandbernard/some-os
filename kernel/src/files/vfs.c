
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
#include "task/syscall.h"

VirtualFilesystem global_file_system;

Error initVirtualFileSystem() {
    CHECKED(mountDeviceFiles());
    return simpleError(SUCCESS);
}

VirtualFilesystem* createVirtualFilesystem() {
    return zalloc(sizeof(VirtualFilesystem));
}

static Error freeFilesystemMount(FilesystemMount* mount, bool force) {
    dealloc(mount->path);
    switch (mount->type) {
        case MOUNT_TYPE_FILE: {
            VfsFile* file = (VfsFile*)mount->data;
            file->functions->close(file, NULL);
            return simpleError(SUCCESS);
        }
        case MOUNT_TYPE_FS: {
            VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
            if (!force && filesystem->open_files != 0) {
                return simpleError(EBUSY);
            } else {
                filesystem->functions->free(filesystem, NULL);
                return simpleError(SUCCESS);
            }
        }
        case MOUNT_TYPE_BIND:
            dealloc(mount->data);
            return simpleError(SUCCESS);
        default: 
            panic();
    }
}

void freeVirtualFilesystem(VirtualFilesystem* fs) {
    for (size_t i = 0; i < fs->mount_count; i++) {
        assert(!isError(freeFilesystemMount(&fs->mounts[i], true)));
    }
    dealloc(fs->mounts);
    dealloc(fs);
}

Error mountFilesystem(VirtualFilesystem* fs, VfsFilesystem* filesystem, const char* path) {
    assert(path[0] == '/');
    lockTaskLock(&fs->lock);
    fs->mounts = krealloc(fs->mounts, (fs->mount_count + 1) * sizeof(FilesystemMount));
    fs->mounts[fs->mount_count].type = MOUNT_TYPE_FS;
    fs->mounts[fs->mount_count].path = stringClone(path);
    fs->mounts[fs->mount_count].data = filesystem;
    fs->mount_count++;
    unlockTaskLock(&fs->lock);
    return simpleError(SUCCESS);
}

Error mountFile(VirtualFilesystem* fs, VfsFile* file, const char* path) {
    assert(path[0] == '/');
    lockTaskLock(&fs->lock);
    fs->mounts = krealloc(fs->mounts, (fs->mount_count + 1) * sizeof(FilesystemMount));
    fs->mounts[fs->mount_count].type = MOUNT_TYPE_FILE;
    fs->mounts[fs->mount_count].path = stringClone(path);
    fs->mounts[fs->mount_count].data = file;
    fs->mount_count++;
    unlockTaskLock(&fs->lock);
    return simpleError(SUCCESS);
}

Error mountRedirect(VirtualFilesystem* fs, const char* from, const char* to) {
    assert(from[0] == '/' && to[0] == '/');
    lockTaskLock(&fs->lock);
    fs->mounts = krealloc(fs->mounts, (fs->mount_count + 1) * sizeof(FilesystemMount));
    fs->mounts[fs->mount_count].type = MOUNT_TYPE_BIND;
    fs->mounts[fs->mount_count].path = stringClone(to);
    fs->mounts[fs->mount_count].data = stringClone(from);
    fs->mount_count++;
    unlockTaskLock(&fs->lock);
    return simpleError(SUCCESS);
}

Error umount(VirtualFilesystem* fs, const char* from) {
    lockTaskLock(&fs->lock);
    for (size_t i = fs->mount_count; i > 0;) {
        i--;
        if (strcmp(fs->mounts[i].path, from) == 0) {
            CHECKED(freeFilesystemMount(&fs->mounts[i], false), unlockTaskLock(&fs->lock));
            fs->mount_count--;
            memmove(fs->mounts + i, fs->mounts + i + 1, fs->mount_count - i);
            unlockTaskLock(&fs->lock);
            return simpleError(SUCCESS);
        }
    }
    if (fs->parent != NULL) {
        CHECKED(umount(fs->parent, from), unlockTaskLock(&fs->lock));
        unlockTaskLock(&fs->lock);
        return simpleError(SUCCESS);
    } else {
        unlockTaskLock(&fs->lock);
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

Error vfsOpen(VirtualFilesystem* fs, struct Process_s* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret) {
    lockTaskLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, stringClone(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                VfsFile* file = (VfsFile*)mount->data;
                unlockTaskLock(&fs->lock);
                CHECKED(file->functions->dup(file, process, ret), unlockTaskLock(&fs->lock));
                if (MODE_TYPE(file->mode) == VFS_TYPE_FIFO) {
                    // This is a fifo, we have to create one
                    *ret = (VfsFile*)createFifoFile(path, file->mode, file->uid, file->gid);
                    file->functions->close(file, NULL);
                    if (*ret != NULL) {
                        return simpleError(SUCCESS);
                    } else {
                        return simpleError(ENOMEM);
                    }
                } else {
                    return simpleError(SUCCESS);
                }
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                if (filesystem->functions->open != NULL) {
                    unlockTaskLock(&fs->lock);
                    char* path_copy = toMountPath(mount, path);
                    Error res = filesystem->functions->open(filesystem, process, path_copy, flags, mode, ret);
                    dealloc(path_copy);
                    return res;
                } else {
                    unlockTaskLock(&fs->lock);
                    return simpleError(EPERM);
                }
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        unlockTaskLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

Error vfsMknod(VirtualFilesystem* fs, struct Process_s* process, const char* path, VfsMode mode, DeviceId dev) {
    lockTaskLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, stringClone(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                unlockTaskLock(&fs->lock);
                return simpleError(EPERM);
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                if (filesystem->functions->mknod != NULL) {
                    unlockTaskLock(&fs->lock);
                    char* path_copy = toMountPath(mount, path);
                    Error res = filesystem->functions->mknod(filesystem, process, path_copy, mode, dev);
                    dealloc(path_copy);
                    return res;
                } else {
                    unlockTaskLock(&fs->lock);
                    return simpleError(EPERM);
                }
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        unlockTaskLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

Error vfsUnlink(VirtualFilesystem* fs, struct Process_s* process, const char* path) {
    lockTaskLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, stringClone(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                unlockTaskLock(&fs->lock);
                return simpleError(EPERM);
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                unlockTaskLock(&fs->lock);
                if (filesystem->functions->unlink != NULL) {
                    char* path_copy = toMountPath(mount, path);
                    Error res = filesystem->functions->unlink(filesystem, process, path_copy);
                    dealloc(path_copy);
                    return res;
                } else {
                    return simpleError(EPERM);
                }
                break;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        unlockTaskLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

Error vfsLink(VirtualFilesystem* fs, struct Process_s* process, const char* old, const char* new) {
    lockTaskLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, stringClone(old));
    FilesystemMount* mount_new = findMountHandling(fs, stringClone(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Linking across filesystem boundaries is impossible
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            unlockTaskLock(&fs->lock);
            if (filesystem->functions->link != NULL) {
                char* old_copy = toMountPath(mount_old, old);
                char* new_copy = toMountPath(mount_new, new);
                Error res = filesystem->functions->link(filesystem, process, old_copy, new_copy);
                dealloc(old_copy);
                dealloc(new_copy);
                return res;
            } else {
                return simpleError(EPERM);
            }
        } else {
            unlockTaskLock(&fs->lock);
            return simpleError(EXDEV);
        }
    } else {
        unlockTaskLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

Error vfsRename(VirtualFilesystem* fs, struct Process_s* process, const char* old, const char* new) {
    // Could just be a link and unlink.
    lockTaskLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, stringClone(old));
    FilesystemMount* mount_new = findMountHandling(fs, stringClone(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Moving across filesystem boundaries requires copying
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            unlockTaskLock(&fs->lock);
            if (filesystem->functions->link != NULL) {
                char* old_copy = toMountPath(mount_old, old);
                char* new_copy = toMountPath(mount_new, new);
                Error res = filesystem->functions->rename(filesystem, process, old_copy, new_copy);
                dealloc(old_copy);
                dealloc(new_copy);
                return res;
            } else {
                return simpleError(EPERM);
            }
        } else {
            unlockTaskLock(&fs->lock);
            return simpleError(EXDEV);
        }
    } else {
        unlockTaskLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

Error vfsReadAt(VfsFile* file, struct Process_s* process, VirtPtr ptr, size_t size, size_t offset, size_t* ret) {
    size_t off = 0;
    CHECKED(file->functions->seek(file, process, offset, VFS_SEEK_SET, &off));
    if (offset != off) {
        return simpleError(EIO);
    } else {
        size_t len = 1;
        while (size != 0 && len != 0) {
            CHECKED(file->functions->read(file, process, ptr, size, &len));
            ptr.address += len;
            size -= len;
        }
        return simpleError(SUCCESS);
    }
}

Error vfsWriteAt(VfsFile* file, struct Process_s* process, VirtPtr ptr, size_t size, size_t offset, size_t* ret) {
    size_t off = 0;
    CHECKED(file->functions->seek(file, process, offset, VFS_SEEK_SET, &off));
    if (offset != off) {
        return simpleError(EIO);
    } else {
        size_t len = 1;
        while (size != 0 && len != 0) {
            CHECKED(file->functions->write(file, process, ptr, size, &len));
            ptr.address += len;
            size -= len;
        }
        return simpleError(SUCCESS);
    }
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

Error createFilesystemFrom(VirtualFilesystem* fs, const char* path, const char* type, VirtPtr data, VfsFilesystem** ret) {
    VfsFile* file;
    CHECKED(vfsOpen(fs, NULL, path, VFS_OPEN_READ, 0, &file));
    if (strcmp(type, "minix") == 0) {
        VfsFilesystem* fs = (VfsFilesystem*)createMinixFilesystem(file, data);
        CHECKED(fs->functions->init(fs, NULL), fs->functions->free(fs, NULL));
        *ret = fs;
        return simpleError(SUCCESS);
    } else {
        file->functions->close(file, NULL);
        return simpleError(EINVAL);
    }
}

