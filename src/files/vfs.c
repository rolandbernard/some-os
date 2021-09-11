
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

static void internalVfsOpen(VirtualFilesystem* fs, Uid uid, Gid gid, char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&fs->lock);
    for (size_t i = fs->mount_count; i != 0;) {
        i--;
        const char* mount_path = fs->mounts[i].path;
        size_t mount_path_length = strlen(mount_path);
        if (strncmp(mount_path, path, mount_path_length) == 0 && (path[mount_path_length] == 0 || path[mount_path_length] == '/')) {
            switch (fs->mounts[i].type) {
                case MOUNT_TYPE_FILE: {
                    VfsFile* file = (VfsFile*)fs->mounts[i].data;
                    unlockSpinLock(&fs->lock);
                    file->functions->dup(file, uid, gid, callback, udata);
                    dealloc(path);
                    return;
                }
                case MOUNT_TYPE_FS: {
                    VfsFilesystem* filesystem = (VfsFilesystem*)fs->mounts[i].data;
                    unlockSpinLock(&fs->lock);
                    filesystem->functions->open(filesystem, uid, gid, path, flags, mode, callback, udata);
                    dealloc(path);
                    return;
                }
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
    unlockSpinLock(&fs->lock);
    if (fs->parent != NULL) {
        vfsOpen(fs->parent, uid, gid, path, flags, mode, callback, udata);
    } else {
        callback(simpleError(NO_SUCH_FILE), NULL, udata);
    }
    dealloc(path);
}

void vfsOpen(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata) {
    char* path_copy = reducedPathCopy(path);
    internalVfsOpen(fs, uid, gid, path_copy, flags, mode, callback, udata);
}

void vfsUnlink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata);

void vfsLink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

void vfsRename(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

