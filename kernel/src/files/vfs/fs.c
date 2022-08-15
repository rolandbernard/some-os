
#include <string.h>

#include "files/vfs/fs.h"

#include "files/path.h"
#include "files/vfs/file.h"
#include "files/vfs/node.h"
#include "files/vfs/super.h"
#include "memory/kalloc.h"

static VfsNode* vfsFollowMounts(VfsNode* curr) {
    lockSpinLock(&curr->lock);
    if (curr->mounted != NULL) {
        lockSpinLock(&curr->mounted->lock);
        VfsNode* ret = curr->mounted->root_node;
        vfsNodeCopy(ret);
        unlockSpinLock(&curr->mounted->lock);
        vfsNodeClose(curr);
        return ret;
    } else {
        unlockSpinLock(&curr->lock);
        return curr;
    }
}

static Error vfsLookupNode(Process* process, VfsNode* curr, const char* path, VfsNode** ret) {
    Error err = simpleError(SUCCESS);
    vfsNodeCopy(curr);
    size_t dirs_capacity = 64;
    size_t dirs_count = 1;
    VfsNode** dirs = kalloc(dirs_capacity * sizeof(VfsNode*));
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
            if (dirs_count > 1) {
                vfsNodeClose(curr);
                dirs_count--;
                curr = dirs[dirs_count];
            }
        } else {
            curr = vfsFollowMounts(curr);
            size_t node_id;
            err = curr->functions->lookup(curr, segment, &node_id);
            if (isError(err)) {
                break;
            }
        }
    }
    for (size_t i = 0; i < dirs_count; i++) {
        vfsNodeClose(dirs[i]);
    }
    dealloc(dirs);
    if (isError(err)) {
        vfsNodeClose(curr);
    } else {
        *ret = curr;
    }
    return err;
}

static Error vfsOpen(VirtualFilesystem* fs, Process* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret) {
    // This is an absolute path
    lockSpinLock(&fs->lock);
    VfsSuperblock* root_mount = fs->root_mounted;
    if (root_mount != NULL) {
        vfsSuperCopy(root_mount);
        unlockSpinLock(&fs->lock);
        VfsNode* root = root_mount->root_node;
        vfsNodeCopy(root);
        vfsSuperClose(root_mount);
        VfsNode* node;
        Error err = vfsLookupNode(process, root, path, &node);
        vfsNodeClose(root);
        if (err.kind == ENOENT && (flags & VFS_OPEN_CREATE) != 0) {
        } else if (isError(err)) {
        } else if ((flags & VFS_OPEN_EXCL) != 0) {
            return simpleError(EEXIST);
        } else {
            return simpleError(SUCCESS);
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

