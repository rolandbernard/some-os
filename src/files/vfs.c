
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

static void freeFilesystemMount(FilesystemMount* mount) {
    dealloc(mount->path);
    switch (mount->type) {
        case MOUNT_TYPE_FILE: {
            VfsFile* file = (VfsFile*)mount->data;
            file->functions->close(file, 0, 0, freeFilesystem, file);
            break;
        }
        case MOUNT_TYPE_FS: {
            VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
            filesystem->functions->free(filesystem, 0, 0, freeFilesystem, filesystem);
            break;
        }
        case MOUNT_TYPE_BIND:
            dealloc(mount->data);
            break;
    }
}

void freeVirtualFilesystem(VirtualFilesystem* fs) {
    lockSpinLock(&fs->lock);
    for (size_t i = 0; i < fs->mount_count; i++) {
        freeFilesystemMount(&fs->mounts[i]);
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
            fs->mount_count--;
            freeFilesystemMount(&fs->mounts[i]);
            memmove(fs->mounts + i, fs->mounts + i + 1, fs->mount_count - i);
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
                    if (strcmp(mount_path, path) == 0) { // Only an exact match is possible here
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
    if (fs->parent != NULL) {
        return findMountHandling(fs->parent, path);
    } else {
        dealloc(path);
        return NULL;
    }
}

void vfsOpen(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, reducedPathCopy(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                VfsFile* file = (VfsFile*)mount->data;
                file->functions->dup(file, uid, gid, callback, udata);
                break;
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                filesystem->functions->open(filesystem, uid, gid, path, flags, mode, callback, udata);
                break;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        callback(simpleError(NO_SUCH_FILE), NULL, udata);
    }
    unlockSpinLock(&fs->lock);
}

void vfsUnlink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount = findMountHandling(fs, reducedPathCopy(path));
    if (mount != NULL) {
        switch (mount->type) {
            case MOUNT_TYPE_FILE: {
                callback(simpleError(UNSUPPORTED), udata);
                break;
            }
            case MOUNT_TYPE_FS: {
                VfsFilesystem* filesystem = (VfsFilesystem*)mount->data;
                filesystem->functions->unlink(filesystem, uid, gid, path, callback, udata);
                break;
            }
            case MOUNT_TYPE_BIND: panic(); // Can't happen. Would return NULL.
        }
    } else {
        callback(simpleError(NO_SUCH_FILE), udata);
    }
    unlockSpinLock(&fs->lock);
}

void vfsLink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, reducedPathCopy(old));
    FilesystemMount* mount_new = findMountHandling(fs, reducedPathCopy(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Linking across filesystem boundaries is impossible
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            filesystem->functions->link(filesystem, uid, gid, old, new, callback, udata);
        } else {
            callback(simpleError(UNSUPPORTED), udata);
        }
    } else {
        callback(simpleError(NO_SUCH_FILE), udata);
    }
    unlockSpinLock(&fs->lock);
}

void vfsRename(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&fs->lock);
    FilesystemMount* mount_old = findMountHandling(fs, reducedPathCopy(old));
    FilesystemMount* mount_new = findMountHandling(fs, reducedPathCopy(new));
    if (mount_old != NULL && mount_new != NULL) {
        if (mount_new == mount_old && mount_new->type == MOUNT_TYPE_FS) {
            // Moving across filesystem boundaries requires copying
            VfsFilesystem* filesystem = (VfsFilesystem*)mount_old->data;
            filesystem->functions->rename(filesystem, uid, gid, old, new, callback, udata);
        } else {
            callback(simpleError(UNSUPPORTED), udata);
        }
    } else {
        callback(simpleError(NO_SUCH_FILE), udata);
    }
    unlockSpinLock(&fs->lock);
}

