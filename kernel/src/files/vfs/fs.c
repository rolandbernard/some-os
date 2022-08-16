
#include <assert.h>
#include <string.h>

#include "files/vfs/fs.h"

#include "files/path.h"
#include "files/vfs/file.h"
#include "files/vfs/node.h"
#include "files/vfs/super.h"
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
    const char** segs = kalloc(dirs_capacity * sizeof(const char*));
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
            VfsNode* next;
            err = vfsNodeLookup(curr, process, segment, &next);
            if (isError(err)) {
                break;
            }
            if (dirs_count == dirs_capacity) {
                dirs_capacity = dirs_capacity * 3 / 2;
                dirs = krealloc(dirs, dirs_capacity * sizeof(VfsNode*));
                segs = krealloc(segs, dirs_capacity * sizeof(const char*));
            }
            dirs[dirs_count] = curr;
            segs[dirs_count - 1] = segment;
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
        dealloc(segs);
        dealloc(path_clone);
        *real_path = path;
        *ret = curr;
    }
    return err;
}

static Error vfsCreateNewNode(Process* process, VfsNode* root, const char* path, VfsMode mode, VfsNode** ret, char** real_path) {
    char* parent_path = getParentPath(path);
    VfsNode* parent;
    char* real_parent_path;
    CHECKED(vfsLookupNode(process, root, parent_path, &parent, &real_parent_path), dealloc(parent_path))
    dealloc(parent_path);
    // TODO
}

static Error vfsOpenNode(Process* process, VfsNode* node, char* path, VfsOpenFlags flags, VfsFile** ret) {
    // TODO
}

static Error vfsOpen(VirtualFilesystem* fs, Process* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret) {
    assert(path[0] == '/');
    lockSpinLock(&fs->lock);
    VfsSuperblock* root_mount = fs->root_mounted;
    if (root_mount != NULL) {
        vfsSuperCopy(root_mount);
        unlockSpinLock(&fs->lock);
        VfsNode* root = root_mount->root_node;
        vfsNodeCopy(root);
        vfsSuperClose(root_mount);
        VfsNode* node;
        char* real_path;
        Error err = vfsLookupNode(process, root, path, &node, &real_path);
        if (err.kind == ENOENT && (flags & VFS_OPEN_CREATE) != 0) {
            // Open can open special files but not create them.
            VfsMode file_mode = (mode & ~VFS_MODE_TYPE) | TYPE_MODE(VFS_TYPE_REG);
            Error err = vfsCreateNewNode(process, root, path, file_mode, &node, &real_path);
            vfsNodeClose(root);
            if (isError(err)) {
                vfsNodeClose(node);
                dealloc(real_path);
                return err;
            } else {
                return vfsOpenNode(process, node, real_path, flags, ret);
            }
        } else {
            vfsNodeClose(root);
            if (isError(err)) {
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
    } else {
        unlockSpinLock(&fs->lock);
        return simpleError(ENOENT);
    }
}

Error vfsOpenAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret) {
    if (path[0] == '/') {
        return vfsOpen(fs, process, path, flags, mode, ret);
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
        Error err = vfsOpen(fs, process, absolute_path, flags, mode, ret);
        dealloc(absolute_path);
        return err;
    }
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

