
#include <assert.h>
#include <string.h>

#include "files/vfs/fs.h"

#include "files/path.h"
#include "files/special/blkfile.h"
#include "files/special/fifo.h"
#include "files/special/pipe.h"
#include "files/special/ttyfile.h"
#include "files/vfs/file.h"
#include "files/vfs/node.h"
#include "files/vfs/super.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "util/util.h"

static VfsNode* vfsFollowMounts(VfsNode* curr) {
    lockSpinLock(&curr->lock);
    if (curr->mounted != NULL) {
        lockSpinLock(&curr->mounted->lock);
        VfsNode* ret = curr->mounted->root_node;
        vfsNodeCopy(ret);
        unlockSpinLock(&curr->mounted->lock);
        unlockSpinLock(&curr->lock);
        vfsNodeClose(curr);
        return vfsFollowMounts(ret);
    } else {
        unlockSpinLock(&curr->lock);
        return curr;
    }
}

static Error vfsLookupNode(Process* process, VfsNode* curr, const char* path, VfsNode** ret, char** real_path) {
    Error err = simpleError(SUCCESS);
    vfsNodeCopy(curr);
    size_t dirs_capacity = 64;
    size_t dirs_count = 1;
    VfsNode** dirs = kalloc(dirs_capacity * sizeof(VfsNode*));
    const char** segs = real_path != NULL ? kalloc(dirs_capacity * sizeof(const char*)) : NULL;
    dirs[0] = curr;
    vfsNodeCopy(curr);
    char* path_clone = stringClone(path);
    char* segments = path_clone;
    while (segments[0] == '/') {
        segments++;
    }
    while (segments[0] != 0) {
        const char* segment = segments;
        while (segments[0] != 0 && segments[0] != '/') {
            segments++;
        }
        if (segments[0] == '/') {
            segments[0] = 0;
            segments++;
        }
        while (segments[0] == '/') {
            segments++;
        }
        if (strcmp(segment, ".") == 0) {
            // Noop?
        } else if (strcmp(segment, "..") == 0) {
            // TODO: Maybe lookup the '..' entry instead?
            if (dirs_count > 1) {
                vfsNodeClose(curr);
                dirs_count--;
                curr = dirs[dirs_count];
            }
        } else {
            curr = vfsFollowMounts(curr);
            // if curr == symbolic link -> Do magic
            VfsNode* next;
            err = vfsNodeLookup(curr, process, segment, &next);
            if (isError(err)) {
                break;
            }
            if (dirs_count == dirs_capacity) {
                dirs_capacity = dirs_capacity * 3 / 2;
                dirs = krealloc(dirs, dirs_capacity * sizeof(VfsNode*));
                segs = real_path != NULL ? krealloc(segs, dirs_capacity * sizeof(const char*)) : NULL;
            }
            dirs[dirs_count] = curr;
            if (real_path != NULL) {
                segs[dirs_count - 1] = segment;
            }
            dirs_count++;
            curr = next;
        }
    }
    for (size_t i = 0; i < dirs_count; i++) {
        vfsNodeClose(dirs[i]);
    }
    dealloc(dirs);
    if (isError(err)) {
        dealloc(segs);
        dealloc(path_clone);
        vfsNodeClose(curr);
    } else {
        if (real_path != NULL) {
            size_t path_length = 0;
            for (size_t i = 0; i < dirs_count - 1; i++) {
                path_length += 1 + strlen(segs[i]);
            }
            char* path = kalloc(umax(1, path_length) + 1);
            path[0] = '/';
            path[umax(1, path_length)] = 0;
            path_length = 0;
            for (size_t i = 0; i < dirs_count - 1; i++) {
                path[path_length] = '/';
                memcpy(path + 1 + path_length, segs, strlen(segs[i]));
                path_length += 1 + strlen(segs[i]);
            }
            *real_path = path;
        }
        dealloc(segs);
        dealloc(path_clone);
        *ret = curr;
    }
    return err;
}

static Error vfsLookupNodeAtAbs(
    VirtualFilesystem* fs, Process* process, const char* path, VfsNode** ret, char** real_path
) {
    lockSpinLock(&fs->lock);
    VfsSuperblock* root_mount = fs->root_mounted;
    if (root_mount != NULL) {
        vfsSuperCopy(root_mount);
        unlockSpinLock(&fs->lock);
        VfsNode* root = root_mount->root_node;
        vfsNodeCopy(root);
        vfsSuperClose(root_mount);
        Error err = vfsLookupNode(process, root, path, ret, real_path);
        vfsNodeClose(root);
        return err;
    } else {
        unlockSpinLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

static Error vfsLookupNodeAt(
    VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsNode** ret, char** real_path
) {
    if (path[0] == '/') {
        return vfsLookupNodeAtAbs(fs, process, path, ret, real_path);
    } else {
        char* absolute_path;
        size_t cwd_length;
        size_t path_length = strlen(path);
        if (file == NULL) {
            lockSpinLock(&process->resources.lock);
            cwd_length = strlen(process->resources.cwd);
            absolute_path = kalloc(cwd_length + path_length + 2);
            memcpy(absolute_path, process->resources.cwd, cwd_length);
            unlockSpinLock(&process->resources.lock);
        } else {
            cwd_length = strlen(file->path);
            absolute_path = kalloc(cwd_length + path_length + 2);
            memcpy(absolute_path, file->path, cwd_length);
        }
        absolute_path[cwd_length] = '/';
        memcpy(absolute_path + cwd_length + 1, path, path_length);
        absolute_path[cwd_length + path_length + 1] = 0;
        Error err = vfsLookupNodeAtAbs(fs, process, absolute_path, ret, real_path);
        dealloc(absolute_path);
        return err;
    }
}

static Error vfsCreateDirectoryDotAndDotDot(VfsNode* dir_node, VfsNode* parent) {
    // TODO: Should this be handled by the concrete filesystem?
    // These links are created by the system regardless of user permissions.
    CHECKED(vfsNodeLink(dir_node, NULL, ".", dir_node));
    CHECKED(vfsNodeLink(dir_node, NULL, "..", parent));
    return simpleError(SUCCESS);
}

static Error vfsCreateNewNode(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsMode mode, VfsNode** ret, char** real_path) {
    char* parent_path = getParentPath(path);
    VfsNode* parent;
    char* real_parent_path = NULL;
    CHECKED(vfsLookupNodeAt(fs, process, file, parent_path, &parent, real_path != NULL ? &real_parent_path : NULL), {
        dealloc(parent_path);
    });
    dealloc(parent_path);
    VfsNode* new;
    CHECKED(vfsSuperNewNode(parent->superblock, &new), {
        vfsNodeClose(parent);
        dealloc(real_parent_path);
    });
    new->mode = mode;
    new->uid = process != NULL ? process->resources.uid : 0;
    new->gid = process != NULL ? process->resources.gid : 0;
    new->st_atime = getNanoseconds();
    new->st_ctime = getNanoseconds();
    new->st_mtime = getNanoseconds();
    const char* filename = getBaseFilename(path);
    CHECKED(vfsNodeLink(parent, process, filename, new), {
        vfsNodeClose(parent);
        vfsNodeClose(new);
        dealloc(real_parent_path);
    });
    if (MODE_TYPE(mode) == VFS_TYPE_DIR) {
        CHECKED(vfsCreateDirectoryDotAndDotDot(new, parent), {
            vfsNodeClose(parent);
            vfsNodeClose(new);
            dealloc(real_parent_path);
        });
    }
    vfsNodeClose(parent);
    *ret = new;
    if (real_path != NULL) {
        size_t parent_path_length = strlen(real_parent_path);
        size_t filename_length = strlen(filename);
        *real_path = kalloc(parent_path_length + 1 + filename_length);
        memcpy(*real_path, real_parent_path, parent_path_length);
        if (parent_path_length == 1) {
            memcpy(*real_path + 1, filename, filename_length);
        } else {
            (*real_path)[parent_path_length] = '/';
            memcpy(*real_path + parent_path_length + 1, filename, filename_length);
        }
    }
    dealloc(real_parent_path);
    return simpleError(SUCCESS);
}

static VfsFile* vfsCreateFile(VfsNode* node, char* path, size_t offset) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->kind = VFS_FILE_NORMAL;
    file->node = node;
    file->path = path;
    file->offset = offset;
    initSpinLock(&file->lock);
    file->ref_count = 1;
    return file;
}

static Error vfsOpenNode(Process* process, VfsNode* node, char* path, VfsOpenFlags flags, VfsFile** ret) {
    if ((flags & VFS_OPEN_DIRECTORY) != 0 && MODE_TYPE(node->mode) != VFS_TYPE_DIR) {
        vfsNodeClose(node);
        dealloc(path);
        return simpleError(ENOTDIR);
    } else if ((flags & VFS_OPEN_REGULAR) != 0 && MODE_TYPE(node->mode) != VFS_TYPE_REG) {
        vfsNodeClose(node);
        dealloc(path);
        return simpleError(EISDIR);
    } else if (!canAccess(node->mode, node->uid, node->gid, process, OPEN_ACCESS(flags))) {
        vfsNodeClose(node);
        dealloc(path);
        return simpleError(EACCES);
    } else if (MODE_TYPE(node->mode) == VFS_TYPE_FIFO) {
        *ret = createFifoFile(node, path);
        return simpleError(SUCCESS);
    } else if (MODE_TYPE(node->mode) == VFS_TYPE_CHAR || MODE_TYPE(node->mode) == VFS_TYPE_BLOCK) {
        if (node->device != NULL && node->device->type == DEVICE_BLOCK && MODE_TYPE(node->mode) == VFS_TYPE_BLOCK) {
            BlockDevice* dev = (BlockDevice*)node->device;
            *ret = createBlockDeviceFile(node, dev, path, (flags & VFS_OPEN_APPEND) != 0 ? node->size : 0);
            return simpleError(SUCCESS);
        } else if (node->device != NULL && node->device->type == DEVICE_BLOCK && MODE_TYPE(node->mode) == VFS_TYPE_CHAR) {
            TtyDevice* dev = (TtyDevice*)node->device;
            *ret = createTtyDeviceFile(node, dev, path);
            return simpleError(SUCCESS);
        } else {
            vfsNodeClose(node);
            dealloc(path);
            return simpleError(ENXIO);
        }
    } else {
        if ((flags & VFS_OPEN_TRUNC) != 0 && (flags & VFS_OPEN_WRITE) != 0) {
            CHECKED(vfsNodeTrunc(node, process, 0), {
                vfsNodeClose(node);
                dealloc(path);
            });
        }
        *ret = vfsCreateFile(node, path, (flags & VFS_OPEN_APPEND) != 0 ? node->size : 0);
        return simpleError(SUCCESS);
    }
}

Error vfsOpenAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret) {
    VfsNode* node;
    char* real_path;
    Error err = vfsLookupNodeAt(fs, process, file, path, &node, &real_path);
    if (err.kind == ENOENT && (flags & VFS_OPEN_CREATE) != 0) {
        // Open can open special files but not create them.
        VfsMode file_mode = (mode & ~VFS_MODE_TYPE) | TYPE_MODE(VFS_TYPE_REG);
        Error err = vfsCreateNewNode(fs, process, file, path, file_mode, &node, &real_path);
        if (isError(err)) {
            vfsNodeClose(node);
            dealloc(real_path);
            return err;
        } else {
            return vfsOpenNode(process, node, real_path, flags, ret);
        }
    } else if (isError(err)) {
        vfsNodeClose(node);
        dealloc(real_path);
        return err;
    } else if ((flags & VFS_OPEN_EXCL) != 0) {
        vfsNodeClose(node);
        dealloc(real_path);
        return simpleError(EEXIST);
    } else {
        return vfsOpenNode(process, node, real_path, flags, ret);
    }
}

Error vfsMknodAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsMode mode, DeviceId id) {
    if (MODE_TYPE(mode) != VFS_TYPE_REG && MODE_TYPE(mode) != VFS_TYPE_DIR && MODE_TYPE(mode) != VFS_TYPE_FIFO) {
        // All of these are unspecified in posix. We don't support them.
        return simpleError(EINVAL);
    } else {
        VfsNode* node;
        Error err = vfsLookupNodeAt(fs, process, file, path, &node, NULL);
        if (err.kind == ENOENT) {
            Error err = vfsCreateNewNode(fs, process, file, path, mode, &node, NULL);
            if (isError(err)) {
                return err;
            } else {
                vfsNodeClose(node);
                return simpleError(SUCCESS);
            }
        } else if (isError(err)) {
            return err;
        } else {
            vfsNodeClose(node);
            return simpleError(EEXIST);
        }
    }
}

static Error vfsRemoveDirectoryDotAndDotDot(Process* process, VfsNode* parent, VfsNode* dir) {
    size_t max_size = sizeof(VfsDirectoryEntry) + 3;
    size_t offset = 0;
    size_t tmp_size = 0;
    VfsDirectoryEntry* entry = kalloc(max_size + 3);
    do {
        CHECKED(vfsNodeReaddirAt(dir, process, virtPtrForKernel(entry), offset, max_size, &tmp_size), dealloc(entry));
        if (entry->len > max_size || (strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0)) {
            dealloc(entry);
            return simpleError(EEXIST);
        }
    } while (tmp_size != 0);
    dealloc(entry);
    lockSpinLock(&dir->lock);
    if (dir->nlinks == 2) {
        dir->nlinks -= 1;
        unlockSpinLock(&dir->lock);
        CHECKED(vfsNodeUnlink(dir, process, "."));
        CHECKED(vfsSuperWriteNode(dir));
        lockSpinLock(&parent->lock);
        parent->nlinks -= 1;
        unlockSpinLock(&parent->lock);
        CHECKED(vfsNodeUnlink(dir, process, ".."));
        CHECKED(vfsSuperWriteNode(parent));
        return simpleError(SUCCESS);
    } else {
        unlockSpinLock(&dir->lock);
        // If we are here, there must be more than one link to this directory.
        // This is not well support right now. (".." entry might be wrong now.)
        return simpleError(SUCCESS);
    }
}

Error vfsUnlinkFrom(Process* process, VfsNode* parent, const char* filename, VfsNode* node) {
    if (MODE_TYPE(node->mode) == VFS_TYPE_DIR) {
        CHECKED(vfsNodeUnlink(parent, process, filename));
        CHECKED(vfsRemoveDirectoryDotAndDotDot(process, parent, node));
    } else {
        CHECKED(vfsNodeUnlink(parent, process, filename));
    }
    lockSpinLock(&node->lock);
    node->nlinks--;
    unlockSpinLock(&node->lock);
    return vfsSuperWriteNode(node);
}

Error vfsUnlinkAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsUnlinkFlags flags) {
    char* parent_path = getParentPath(path);
    VfsNode* parent;
    CHECKED(vfsLookupNodeAt(fs, process, file, parent_path, &parent, NULL), dealloc(parent_path));
    dealloc(parent_path);
    const char* filename = getBaseFilename(path);
    VfsNode* node;
    CHECKED(vfsNodeLookup(parent, process, filename, &node), vfsNodeClose(parent));
    Error err = vfsUnlinkFrom(process, parent, filename, node);
    vfsNodeClose(node);
    vfsNodeClose(parent);
    return err;
}

Error vfsLinkAt(VirtualFilesystem* fs, Process* process, VfsFile* old_file, const char* old, VfsFile* new_file, const char* new) {
}

Error vfsMount(VirtualFilesystem* fs, const char* path, VfsSuperblock* sb) {
}

Error vfsUmount(VirtualFilesystem* fs, const char* path) {
}

Error vfsCreateSuperblock(VirtualFilesystem* fs, const char* path, const char* type, VirtPtr data, VfsSuperblock** ret) {
}

bool canAccess(VfsMode mode, Uid file_uid, Gid file_gid, struct Process_s* process, VfsAccessFlags flags) {
    if (process == NULL || process->resources.uid == 0) {
        // Kernel or uid 0 are allowed to do everything
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

