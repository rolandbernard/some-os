
#include <assert.h>
#include <string.h>

#include "files/vfs/fs.h"

#include "devices/devfs.h"
#include "files/minix/super.h"
#include "files/path.h"
#include "files/special/blkfile.h"
#include "files/special/chrfile.h"
#include "files/special/fifo.h"
#include "files/special/pipe.h"
#include "files/vfs/file.h"
#include "files/vfs/node.h"
#include "files/vfs/super.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "util/util.h"

typedef enum {
    VFS_LOOKUP_NORMAL = 0,
    VFS_LOOKUP_PARENT = (1 << 0),
    VFS_LOOKUP_SKIPLASTMOUNT = (1 << 1),
} VfsLookupFlags;

static VfsNode* vfsFollowMounts(VfsNode* curr, bool last_mountpoint) {
    lockTaskLock(&curr->lock);
    if (curr->mounted != NULL) {
        lockTaskLock(&curr->mounted->lock);
        VfsNode* mounted_root = curr->mounted->root_node;
        vfsNodeCopy(mounted_root);
        unlockTaskLock(&curr->mounted->lock);
        unlockTaskLock(&curr->lock);
        if (last_mountpoint) {
            lockTaskLock(&mounted_root->lock);
            if (mounted_root->mounted != NULL) {
                unlockTaskLock(&mounted_root->lock);
                vfsNodeClose(curr);
                return vfsFollowMounts(mounted_root, last_mountpoint);
            } else {
                unlockTaskLock(&mounted_root->lock);
                vfsNodeClose(mounted_root);
                return curr;
            }
        } else {
            return vfsFollowMounts(mounted_root, last_mountpoint);
        }
    } else {
        unlockTaskLock(&curr->lock);
        return curr;
    }
}

static Error vfsLookupNode(
    Process* process, VfsNode* curr, const char* path, VfsLookupFlags flags, VfsNode** ret, const char* prefix, char** real_path
) {
    Error err = simpleError(SUCCESS);
    vfsNodeCopy(curr);
    size_t dirs_capacity = 64;
    size_t dirs_count = 1;
    VfsNode** dirs = kalloc(dirs_capacity * sizeof(VfsNode*));
    const char** segs = real_path != NULL && prefix != NULL
        ? kalloc(dirs_capacity * sizeof(const char*)) : NULL;
    dirs[0] = curr;
    vfsNodeCopy(curr);
    char* path_clone = stringClone(path);
    char* segments = path_clone;
    while (segments[0] == '/') {
        segments++;
    }
    while (segments[0] != 0) {
        // TODO: if curr == symbolic link -> Do magic
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
        lockTaskLock(&curr->lock);
        if (MODE_TYPE(curr->stat.mode) != VFS_TYPE_DIR) {
            unlockTaskLock(&curr->lock);
            err = simpleError(ENOTDIR);
            break;
        }
        unlockTaskLock(&curr->lock);
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
            if (dirs_count == dirs_capacity) {
                dirs_capacity = dirs_capacity * 3 / 2;
                dirs = krealloc(dirs, dirs_capacity * sizeof(VfsNode*));
                segs = real_path != NULL && prefix != NULL
                    ? krealloc(segs, dirs_capacity * sizeof(const char*)) : NULL;
            }
            vfsNodeCopy(curr);
            dirs[dirs_count] = curr;
            if (real_path != NULL && prefix != NULL) {
                segs[dirs_count - 1] = segment;
            }
            dirs_count++;
            curr = vfsFollowMounts(curr, false);
            VfsNode* next;
            err = vfsNodeLookup(curr, process, segment, &next);
            if (isError(err)) {
                break;
            }
            vfsNodeClose(curr);
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
        curr = vfsFollowMounts(curr, (flags & VFS_LOOKUP_SKIPLASTMOUNT) != 0);
        if (real_path != NULL) {
            if (prefix == NULL) {
                *real_path = NULL;
            } else {
                size_t path_length = strlen(prefix);
                for (size_t i = 0; i < dirs_count - 1; i++) {
                    path_length += 1 + strlen(segs[i]);
                }
                char* path = kalloc(umax(1, path_length) + 1);
                path_length = strlen(prefix);
                memcpy(path, prefix, path_length);
                path[path_length] = '/';
                for (size_t i = 0; i < dirs_count - 1; i++) {
                    path[path_length] = '/';
                    memcpy(path + 1 + path_length, segs[i], strlen(segs[i]));
                    path_length += 1 + strlen(segs[i]);
                }
                path[umax(1, path_length)] = 0;
                *real_path = path;
                dealloc(segs);
            }
        }
        dealloc(path_clone);
        *ret = curr;
    }
    return err;
}

static Error vfsLookupNodeAtExactAbs(
    VirtualFilesystem* fs, Process* process, const char* path, VfsLookupFlags flags, VfsNode** ret, char** real_path
) {
    lockTaskLock(&fs->lock);
    VfsSuperblock* root_mount = fs->root_mounted;
    if (root_mount != NULL) {
        VfsNode* root = root_mount->root_node;
        vfsNodeCopy(root);
        unlockTaskLock(&fs->lock);
        Error err = vfsLookupNode(process, root, path, flags, ret, "", real_path);
        vfsNodeClose(root);
        return err;
    } else {
        // Temporary mounts are only for use until the root filesystem is mounted.
        VfsTmpMount* mount = fs->tmp_mounted;
        while (mount != NULL) {
            size_t prefix_len = strlen(mount->prefix);
            if (strncmp(path, mount->prefix, prefix_len) == 0 && (mount->prefix[prefix_len] == '/' || mount->prefix[prefix_len] == 0)) {
                VfsNode* root = mount->mounted->root_node;
                vfsNodeCopy(root);
                unlockTaskLock(&fs->lock);
                Error err = vfsLookupNode(process, root, path + prefix_len, flags, ret, mount->prefix, real_path);
                vfsNodeClose(root);
                return err;
            }
            mount = mount->next;
        }
        unlockTaskLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

static Error vfsLookupNodeAtAbs(
    VirtualFilesystem* fs, Process* process, const char* path, VfsLookupFlags flags, VfsNode** ret, char** real_path
) {
    if ((flags & VFS_LOOKUP_PARENT) != 0) {
        char* parent_path = getParentPath(path);
        Error err = vfsLookupNodeAtExactAbs(fs, process, parent_path, flags, ret, real_path);
        dealloc(parent_path);
        return err;
    } else {
        return vfsLookupNodeAtExactAbs(fs, process, path, flags, ret, real_path);
    }
}

static Error vfsLookupNodeAt(
    VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsLookupFlags flags, VfsNode** ret, char** real_path
) {
    if (path[0] == '/') {
        return vfsLookupNodeAtAbs(fs, process, path, flags, ret, real_path);
    } else {
        char* absolute_path;
        size_t cwd_length;
        size_t path_length = strlen(path);
        if (file == NULL) {
            assert(process != NULL);
            lockTaskLock(&process->resources.lock);
            cwd_length = strlen(process->resources.cwd);
            absolute_path = kalloc(cwd_length + path_length + 2);
            memcpy(absolute_path, process->resources.cwd, cwd_length);
            unlockTaskLock(&process->resources.lock);
        } else if (file->path != NULL) {
            cwd_length = strlen(file->path);
            absolute_path = kalloc(cwd_length + path_length + 2);
            memcpy(absolute_path, file->path, cwd_length);
        } else {
            return simpleError(EINVAL);
        }
        absolute_path[cwd_length] = '/';
        memcpy(absolute_path + cwd_length + 1, path, path_length);
        absolute_path[cwd_length + path_length + 1] = 0;
        Error err = vfsLookupNodeAtAbs(fs, process, absolute_path, flags, ret, real_path);
        dealloc(absolute_path);
        return err;
    }
}

static Error vfsCreateDirectoryDotAndDotDot(VfsNode* dir_node, VfsNode* parent) {
    // These links are created by the system regardless of user permissions.
    CHECKED(vfsNodeLink(dir_node, NULL, ".", dir_node));
    CHECKED(vfsNodeLink(dir_node, NULL, "..", parent));
    return simpleError(SUCCESS);
}

static Error vfsCreateNewNode(
    VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsMode mode,
    DeviceId device, VfsNode** ret, char** real_path
) {
    VfsNode* parent;
    char* real_parent_path = NULL;
    CHECKED(vfsLookupNodeAt(
        fs, process, file, path, VFS_LOOKUP_PARENT, &parent,
        real_path != NULL ? &real_parent_path : NULL
    ));
    VfsNode* new;
    CHECKED(vfsSuperNewNode(parent->superblock, &new), {
        vfsNodeClose(parent);
        dealloc(real_parent_path);
    });
    new->stat.mode = mode;
    new->stat.nlinks = 0;
    if (process != NULL) {
        lockSpinLock(&process->user.lock);
        new->stat.uid = process->user.euid;
        new->stat.gid = process->user.egid;
        unlockSpinLock(&process->user.lock);
        lockTaskLock(&process->resources.lock);
        new->stat.mode &= ~process->resources.umask;
        unlockTaskLock(&process->resources.lock);
    } else {
        new->stat.uid = 0;
        new->stat.gid = 0;
    }
    new->stat.rdev = device;
    new->stat.size = 0;
    Time time = getNanosecondsWithFallback();
    new->stat.atime = time;
    new->stat.mtime = time;
    new->stat.ctime = time;
    // dev, id, block_size, and blocks must be initialized by the concrete implementation.
    // We don't have to write the node data, it will be written when linking.
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
    file->node = node;
    file->path = path;
    file->ref_count = 1;
    file->offset = offset;
    file->flags = 0;
    initTaskLock(&file->lock);
    initTaskLock(&file->ref_lock);
    return file;
}

static Error vfsOpenNode(Process* process, VfsNode* node, char* path, VfsOpenFlags flags, VfsFile** ret) {
    CHECKED(canAccess(node, process, OPEN_ACCESS(flags)), {
        vfsNodeClose(node);
        dealloc(path);
    });
    if (MODE_TYPE(node->stat.mode) == VFS_TYPE_FIFO) {
        *ret = createFifoFile(node->real_node, path, (flags & VFS_OPEN_WRITE) != 0);
        return simpleError(SUCCESS);
    } else if (MODE_TYPE(node->stat.mode) == VFS_TYPE_CHAR || MODE_TYPE(node->stat.mode) == VFS_TYPE_BLOCK) {
        Device* device = getDeviceWithId(node->stat.rdev);
        if (device != NULL && device->type == DEVICE_BLOCK && MODE_TYPE(node->stat.mode) == VFS_TYPE_BLOCK) {
            BlockDevice* dev = (BlockDevice*)device;
            *ret = createBlockDeviceFile(node->real_node, dev, path, (flags & VFS_OPEN_APPEND) != 0 ? dev->size : 0);
            return simpleError(SUCCESS);
        } else if (device != NULL && device->type == DEVICE_CHAR && MODE_TYPE(node->stat.mode) == VFS_TYPE_CHAR) {
            CharDevice* dev = (CharDevice*)device;
            *ret = createCharDeviceFile(node->real_node, dev, path);
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
        *ret = vfsCreateFile(node, path, (flags & VFS_OPEN_APPEND) != 0 ? node->stat.size : 0);
        return simpleError(SUCCESS);
    }
}

Error vfsOpenAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret) {
    VfsNode* node;
    char* real_path;
    Error err = vfsLookupNodeAt(fs, process, file, path, VFS_LOOKUP_NORMAL, &node, &real_path);
    if (err.kind == ENOENT && (flags & VFS_OPEN_CREAT) != 0) {
        // Open can open special files but not create them.
        VfsMode file_mode = (mode & ~VFS_MODE_TYPE) | TYPE_MODE(VFS_TYPE_REG);
        Error err = vfsCreateNewNode(fs, process, file, path, file_mode, 0, &node, &real_path);
        if (isError(err)) {
            return err;
        } else {
            return vfsOpenNode(process, node, real_path, flags, ret);
        }
    } else if (isError(err)) {
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
        Error err = vfsLookupNodeAt(fs, process, file, path, VFS_LOOKUP_NORMAL, &node, NULL);
        if (err.kind == ENOENT) {
            Error err = vfsCreateNewNode(fs, process, file, path, mode, id, &node, NULL);
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

static Error vfsCheckDirectoryIsEmpty(Process* process, VfsNode* dir) {
    size_t max_size = sizeof(VfsDirectoryEntry) + 3;
    size_t offset = 0;
    size_t tmp_size = 0;
    VfsDirectoryEntry* entry = kalloc(max_size + 3);
    do {
        size_t vfs_size = 0;
        CHECKED(vfsNodeReaddirAt(dir, process, virtPtrForKernel(entry), offset, max_size, &tmp_size, &vfs_size), dealloc(entry));
        if (entry->len > max_size || (strcmp(entry->name, ".") != 0 && strcmp(entry->name, "..") != 0)) {
            dealloc(entry);
            return simpleError(EEXIST);
        }
        offset += tmp_size;
    } while (tmp_size != 0);
    dealloc(entry);
    return simpleError(SUCCESS);
}

static Error vfsRemoveDirectoryDotAndDotDot(Process* process, VfsNode* parent, VfsNode* dir) {
    lockTaskLock(&dir->lock);
    if (dir->stat.nlinks == 1) {
        unlockTaskLock(&dir->lock);
        CHECKED(vfsNodeUnlink(dir, process, ".", dir));
        CHECKED(vfsNodeUnlink(dir, process, "..", parent));
        return simpleError(SUCCESS);
    } else {
        unlockTaskLock(&dir->lock);
        // If we are here, there must be more than one link to this directory.
        // This is not well support right now. (".." entry might be wrong now.)
        return simpleError(SUCCESS);
    }
}

Error vfsUnlinkFrom(Process* process, VfsNode* parent, const char* filename, VfsNode* node) {
    if (MODE_TYPE(node->stat.mode) == VFS_TYPE_DIR) {
        CHECKED(vfsCheckDirectoryIsEmpty(process, node));
        CHECKED(vfsNodeUnlink(parent, process, filename, node));
        return vfsRemoveDirectoryDotAndDotDot(process, parent, node);
    } else {
        return vfsNodeUnlink(parent, process, filename, node);
    }
}

Error vfsUnlinkAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path) {
    VfsNode* parent;
    CHECKED(vfsLookupNodeAt(fs, process, file, path, VFS_LOOKUP_PARENT, &parent, NULL));
    const char* filename = getBaseFilename(path);
    VfsNode* node;
    CHECKED(vfsNodeLookup(parent, process, filename, &node), vfsNodeClose(parent));
    Error err = vfsUnlinkFrom(process, parent, filename, node);
    vfsNodeClose(node);
    vfsNodeClose(parent);
    return err;
}

Error vfsLinkAt(VirtualFilesystem* fs, Process* process, VfsFile* old_file, const char* old, VfsFile* new_file, const char* new) {
    VfsNode* parent;
    CHECKED(vfsLookupNodeAt(fs, process, new_file, new, VFS_LOOKUP_PARENT, &parent, NULL));
    const char* filename = getBaseFilename(new);
    VfsNode* new_node;
    Error err = vfsNodeLookup(parent, process, filename, &new_node);
    if (err.kind == ENOENT) {
        VfsNode* old_node;
        CHECKED(
            vfsLookupNodeAt(fs, process, old_file, old, VFS_LOOKUP_NORMAL, &old_node, NULL),
            vfsNodeClose(parent)
        );
        Error err = vfsNodeLink(parent, process, filename, old_node);
        vfsNodeClose(old_node);
        vfsNodeClose(parent);
        return err;
    } else if (isError(err)) {
        vfsNodeClose(parent);
        return err;
    } else {
        vfsNodeClose(parent);
        vfsNodeClose(new_node);
        return simpleError(EEXIST);
    }
}

Error vfsMount(VirtualFilesystem* fs, Process* process, const char* path, VfsSuperblock* sb) {
    assert(sb != NULL);
    lockTaskLock(&fs->lock);
    if (fs->root_mounted == NULL) {
        char* path_copy = stringClone(path);
        inlineReducePath(path_copy);
        if (strcmp(path_copy, "/") == 0) {
            dealloc(path_copy);
            fs->root_mounted = sb;
            while (fs->tmp_mounted != NULL) {
                VfsTmpMount* mount = fs->tmp_mounted;
                fs->tmp_mounted = mount->next;
                if (isError(vfsMount(fs, NULL, mount->prefix, mount->mounted))) {
                    // If there is an error, we throw away the mount.
                    vfsSuperClose(mount->mounted);
                }
                dealloc(mount->prefix);
                dealloc(mount);
            }
            unlockTaskLock(&fs->lock);
            return simpleError(SUCCESS);
        } else {
            VfsTmpMount* tmp_mount = kalloc(sizeof(VfsTmpMount));
            tmp_mount->next = fs->tmp_mounted;
            tmp_mount->prefix = path_copy;
            tmp_mount->mounted = sb;
            fs->tmp_mounted = tmp_mount;
            unlockTaskLock(&fs->lock);
            return simpleError(SUCCESS);
        }
    } else {
        unlockTaskLock(&fs->lock);
        VfsNode* node;
        CHECKED(vfsLookupNodeAt(fs, process, NULL, path, VFS_LOOKUP_NORMAL, &node, NULL));
        lockTaskLock(&node->lock);
        if (node->mounted != NULL) {
            unlockTaskLock(&node->lock);
            vfsNodeClose(node);
            return simpleError(EINVAL);
        } else if (MODE_TYPE(node->stat.mode) != VFS_TYPE_DIR) {
            unlockTaskLock(&node->lock);
            vfsNodeClose(node);
            return simpleError(ENOTDIR);
        } else {
            node->mounted = sb;
            unlockTaskLock(&node->lock);
            // We keep sb and node referenced until we unmount.
            return simpleError(SUCCESS);
        }
    }
}

Error vfsUmount(VirtualFilesystem* fs, Process* process, const char* path) {
    lockTaskLock(&fs->lock);
    if (fs->root_mounted == NULL) {
        char* path_copy = stringClone(path);
        inlineReducePath(path_copy);
        VfsTmpMount** mount = &fs->tmp_mounted;
        while (*mount != NULL) {
            if (strcmp(path_copy, (*mount)->prefix) == 0) {
                VfsTmpMount* to_remove = *mount;
                *mount = (*mount)->next;
                unlockTaskLock(&fs->lock);
                vfsSuperClose(to_remove->mounted);
                dealloc(to_remove->prefix);
                dealloc(to_remove);
                dealloc(path_copy);
                return simpleError(SUCCESS);
            }
            mount = &(*mount)->next;
        }
        unlockTaskLock(&fs->lock);
        dealloc(path_copy);
        return simpleError(ENOENT);
    } else if (strcmp(path, "/") == 0 && fs->root_mounted->root_node->mounted == NULL) {
        VfsSuperblock* sb = fs->root_mounted;
        fs->root_mounted = NULL;
        unlockTaskLock(&fs->lock);
        vfsSuperClose(sb);
        return simpleError(SUCCESS);
    } else {
        unlockTaskLock(&fs->lock);
        VfsNode* node;
        CHECKED(vfsLookupNodeAt(fs, process, NULL, path, VFS_LOOKUP_SKIPLASTMOUNT, &node, NULL));
        lockTaskLock(&node->lock);
        if (node->mounted == NULL) {
            unlockTaskLock(&node->lock);
            vfsNodeClose(node);
            return simpleError(EINVAL);
        } else {
            vfsSuperClose(node->mounted);
            node->mounted = NULL;
            unlockTaskLock(&node->lock);
            vfsNodeClose(node);
            vfsNodeClose(node); // Close this two times, ones for this lookup and ones for the mount.
            return simpleError(SUCCESS);
        }
    }
}

static size_t fsdrivers_count = 0;
static size_t fsdrivers_capacity = 0;
static VfsFilesystemDriver** fsdrivers;

void vfsRegisterFilesystemDriver(VfsFilesystemDriver* driver) {
    if (fsdrivers_count == fsdrivers_capacity) {
        fsdrivers_capacity = fsdrivers_capacity == 0 ? 8 : fsdrivers_capacity * 4 / 3;
        fsdrivers = krealloc(fsdrivers, fsdrivers_capacity * sizeof(VfsFilesystemDriver*));
    }
    fsdrivers[fsdrivers_count] = driver;
    fsdrivers_count++;
}

Error vfsCreateSuperblock(
    VirtualFilesystem* fs, Process* process, const char* path, const char* type, VirtPtr data, VfsSuperblock** ret
) {
    for (size_t i = 0; i < fsdrivers_count; i++) {
        if (strcmp(type, fsdrivers[i]->name) == 0) {
            VfsFile* file = NULL;
            if ((fsdrivers[i]->flags & VFS_DRIVER_FLAGS_NOFILE) == 0) {
                CHECKED(vfsOpenAt(fs, process, NULL, path, VFS_OPEN_READ, 0, &file));
            }
            Error err = fsdrivers[i]->create_superblock(file, data, ret);
            if (file != NULL) {
                vfsFileClose(file);
            }
            return err;
        }
    }
    return simpleError(EINVAL);
}

static Error basicCanAccess(VfsStat* stat, struct Process_s* process, VfsAccessFlags flags) {
    if (process == NULL || process->user.euid == 0) {
        // Kernel or uid 0 are allowed to do everything
        return simpleError(SUCCESS);
    } else if (
        ((flags & VFS_ACCESS_CHMOD) != 0 || (flags & VFS_ACCESS_CHOWN))
        && stat->uid != process->user.euid
    ) {
        return simpleError(EPERM);
    } else if (
        (flags & VFS_ACCESS_R) != 0 && (stat->mode & VFS_MODE_A_R) == 0
        && ((stat->mode & VFS_MODE_G_R) == 0 || stat->gid != process->user.egid)
        && ((stat->mode & VFS_MODE_O_R) == 0 || stat->uid != process->user.euid)
    ) {
        return simpleError(EACCES);
    } else if (
        (flags & VFS_ACCESS_W) != 0 && (stat->mode & VFS_MODE_A_W) == 0
        && ((stat->mode & VFS_MODE_G_W) == 0 || stat->gid != process->user.egid)
        && ((stat->mode & VFS_MODE_O_W) == 0 || stat->uid != process->user.euid)
    ) {
        return simpleError(EACCES);
    } else if (
        (flags & VFS_ACCESS_X) != 0 && (stat->mode & VFS_MODE_A_X) == 0
        && ((stat->mode & VFS_MODE_G_X) == 0 || stat->gid != process->user.egid)
        && ((stat->mode & VFS_MODE_O_X) == 0 || stat->uid != process->user.euid)
    ) {
        return simpleError(EACCES);
    } else if ((flags & VFS_ACCESS_REG) != 0 && MODE_TYPE(stat->mode) != VFS_TYPE_REG) {
        return simpleError(MODE_TYPE(stat->mode) == VFS_TYPE_DIR ? EISDIR : EINVAL);
    } else if ((flags & VFS_ACCESS_DIR) != 0 && MODE_TYPE(stat->mode) != VFS_TYPE_DIR) {
        return simpleError(ENOTDIR);
    } else {
        return simpleError(SUCCESS);
    }
}

Error canAccess(VfsNode* node, struct Process_s* process, VfsAccessFlags flags) {
    if (process == NULL) {
        return simpleError(SUCCESS);
    } else {
        lockTaskLock(&node->lock);
        lockSpinLock(&process->user.lock);
        Error err = basicCanAccess(&node->stat, process, flags);
        unlockSpinLock(&process->user.lock);
        unlockTaskLock(&node->lock);
        return err;
    }
}

VirtualFilesystem global_file_system;

Error vfsInit(VirtualFilesystem* fs) {
    initTaskLock(&fs->lock);
    fs->root_mounted = NULL;
    fs->tmp_mounted = NULL;
    return simpleError(SUCCESS);
}

Error registerAllFilesystemDrivers() {
    CHECKED(registerFsDriverDevfs());
    CHECKED(registerFsDriverMinix());
    return simpleError(SUCCESS);
}

