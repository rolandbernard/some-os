
#include <stddef.h>
#include <string.h>

#include "files/minix/minix.h"

#include "error/log.h"
#include "files/minix/file.h"
#include "files/minix/maps.h"
#include "files/path.h"
#include "files/vfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "util/util.h"

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode) {
    return (2 + fs->superblock.imap_blocks + fs->superblock.zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

size_t offsetForZone(size_t zone) {
    return zone * MINIX_BLOCK_SIZE;
}

#define MAX_SINGLE_READ_SIZE (1 << 16)

static Error minixFindINodeForNameIn(MinixFile* file, Process* process, const char* name, uint32_t* inodenum, size_t* off, size_t* dir_size) {
    VfsStat stat;
    CHECKED(file->base.functions->stat((VfsFile*)file, NULL, virtPtrForKernel(&stat)));
    if (MODE_TYPE(stat.mode) != VFS_TYPE_DIR) {
        // File is not a directory
        return simpleError(ENOTDIR);
    } else if (!canAccess(stat.mode, stat.uid, stat.gid, process, VFS_ACCESS_X)) {
        return simpleError(EACCES);
    } else {
        *dir_size = stat.size;
        size_t offset = 0;
        size_t left = stat.size;
        MinixDirEntry* tmp_buffer = kalloc(umin(MAX_SINGLE_READ_SIZE, left));
        while (left > 0) {
            size_t tmp_size = umin(MAX_SINGLE_READ_SIZE, left);
            CHECKED(vfsReadAt((VfsFile*)file, process, virtPtrForKernel(tmp_buffer), tmp_size, offset, &tmp_size), {
                dealloc(tmp_buffer);
            });
            if (tmp_size == 0) {
                dealloc(tmp_buffer);
                return simpleError(EIO);
            }
            for (size_t i = 0; i < (tmp_size / sizeof(MinixDirEntry)); i++) {
                if (tmp_buffer[i].inode != 0 && strcmp((char*)tmp_buffer[i].name, name) == 0) {
                    *inodenum = tmp_buffer[i].inode;
                    *off = offset;
                    dealloc(tmp_buffer);
                    return simpleError(SUCCESS);
                }
                offset += sizeof(MinixDirEntry);
            }
            left -= tmp_size;
        }
        dealloc(tmp_buffer);
        return simpleError(ENOENT);
    }
}

static Error minixFindINodeForPath(MinixFilesystem* fs, Process* process, const char* path, uint32_t* inodenum) {
    uint32_t current_inodenum = 1;
    char* path_clone = stringClone(path);
    char* segments = path_clone + (path[0] == '/' ? 1 : 0); // all paths start with /, skip it
    while (*segments != 0) {
        const char* segment = segments;
        while (*segments != 0 && *segments != '/') {
            segments++;
        }
        if (*segments == '/') {
            *segments = 0;
            segments++;
        }
        MinixFile* current_dir = createMinixFileForINode(fs, current_inodenum, true);
        size_t tmp_offset;
        size_t tmp_dir_size;
        CHECKED(minixFindINodeForNameIn(current_dir, process, segment, &current_inodenum, &tmp_offset, &tmp_dir_size), {
            dealloc(path_clone);
        });
        current_dir->base.functions->close((VfsFile*)current_dir);
    }
    dealloc(path_clone);
    *inodenum = current_inodenum;
    return simpleError(SUCCESS);
}

static Error minixOpenInode(MinixFilesystem* fs, Process* process, uint32_t inodenum, VfsOpenFlags flags, VfsFile** ret) {
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, inodenum), &tmp_size));
    if (tmp_size != sizeof(MinixInode)) {
        return simpleError(EIO);
    } else if ((flags & VFS_OPEN_DIRECTORY) != 0 && MODE_TYPE(inode.mode) != VFS_TYPE_DIR) {
        return simpleError(ENOTDIR);
    } else if ((flags & VFS_OPEN_REGULAR) != 0 && MODE_TYPE(inode.mode) != VFS_TYPE_REG) {
        return simpleError(EISDIR);
    } else if (!canAccess(inode.mode, inode.uid, inode.gid, process, OPEN_ACCESS(flags))) {
        return simpleError(EACCES);
    } else {
        MinixFile* file = createMinixFileForINode(fs, inodenum, (flags & VFS_OPEN_DIRECTORY) != 0);
        file->base.mode = inode.mode;
        file->base.uid = inode.uid;
        file->base.gid = inode.gid;
        if ((flags & VFS_OPEN_APPEND) != 0) {
            CHECKED(file->base.functions->seek((VfsFile*)file, process, inode.size, VFS_SEEK_SET, &tmp_size), {
                file->base.functions->close((VfsFile*)file);
            });
            if (tmp_size != inode.size) {
                file->base.functions->close((VfsFile*)file);
                return simpleError(EIO);
            }
        } else if ((flags & VFS_OPEN_TRUNC) != 0) {
            CHECKED(file->base.functions->trunc((VfsFile*)file, process, 0), {
                file->base.functions->close((VfsFile*)file);
            });
        }
        *ret = (VfsFile*)file;
        return simpleError(SUCCESS);
    }
}

static Error minixAppendDirEntryInto(VfsFile* file, Process* process, uint32_t inodenum, const char* name) {
    MinixDirEntry entry = { .inode = inodenum };
    size_t name_len = strlen(name) + 1;
    if (name_len > sizeof(entry.name)) {
        name_len = sizeof(entry.name);
    }
    memcpy(entry.name, name, name_len);
    size_t tmp_size;
    CHECKED(file->functions->write(file, process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), &tmp_size));
    if (tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EIO);
    }
    return simpleError(SUCCESS);
}

static Error minixCreateNewInode(MinixFilesystem* fs, Process* process, VfsMode mode, uint32_t* inodenum) {
    CHECKED(getFreeMinixInode(fs, inodenum));
    MinixInode inode = {
        .atime = getUnixTime(),
        .ctime = getUnixTime(),
        .mtime = getUnixTime(),
        .gid = process != NULL ? process->resources.gid : 0,
        .mode = mode,
        .nlinks = 1,
        .size = 0,
        .uid = process != NULL ? process->resources.uid : 0,
    };
    memset(inode.zones, 0, sizeof(inode.zones));
    size_t tmp_size;
    CHECKED(vfsWriteAt(
        fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, *inodenum), &tmp_size
    ));
    if (tmp_size != sizeof(MinixInode)) {
        return simpleError(EIO);
    }
    return simpleError(SUCCESS);
}

static Error minixCreateNewNodeAt(MinixFilesystem* fs, Process* process, const char* path, VfsMode mode, uint32_t* inodenum) {
    // If we don't find the node itself, try opening the parent
    char* parent_path = getParentPath(path);
    VfsFile* parent;
    CHECKED(fs->base.functions->open(
        (VfsFilesystem*)fs, process, parent_path, VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0, &parent
    ), {
        dealloc(parent_path);
    });
    dealloc(parent_path);
    CHECKED(minixCreateNewInode(fs, process, mode, inodenum), {
        parent->functions->close(parent);
    });
    CHECKED(minixAppendDirEntryInto(parent, process, *inodenum, getBaseFilename(path)), {
        parent->functions->close(parent);
    });
    parent->functions->close(parent);
    return simpleError(SUCCESS);
}

static Error minixOpenFunction(
    MinixFilesystem* fs, Process* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret
) {
    lockTaskLock(&fs->lock);
    uint32_t inodenum;
    Error err = minixFindINodeForPath(fs, process, path, &inodenum);
    if (err.kind == ENOENT && (flags & VFS_OPEN_CREATE) != 0) {
        VfsMode file_mode = (mode & ~VFS_MODE_TYPE) // Don't allow overwriting the actual type
            | ((flags & VFS_OPEN_DIRECTORY) != 0 ? TYPE_MODE(VFS_TYPE_DIR) : TYPE_MODE(VFS_TYPE_REG));
        CHECKED(minixCreateNewNodeAt(fs, process, path, file_mode, &inodenum), unlockTaskLock(&fs->lock));
        CHECKED(minixOpenInode(fs, process, inodenum, flags, ret), unlockTaskLock(&fs->lock));
        unlockTaskLock(&fs->lock);
        return simpleError(SUCCESS);
    } else if (isError(err)) {
        unlockTaskLock(&fs->lock);
        return err;
    } else if ((flags & VFS_OPEN_EXCL) != 0) {
        // The file should not exist
        unlockTaskLock(&fs->lock);
        return simpleError(EEXIST);
    } else {
        CHECKED(minixOpenInode(fs, process, inodenum, flags, ret), unlockTaskLock(&fs->lock));
        unlockTaskLock(&fs->lock);
        return simpleError(SUCCESS);
    }
}

static Error minixMknodFunction(MinixFilesystem* fs, Process* process, const char* path, VfsMode mode, DeviceId dev) {
    lockTaskLock(&fs->lock);
    uint32_t inodenum;
    Error err = minixFindINodeForPath(fs, process, path, &inodenum);
    if (err.kind == ENOENT) {
        CHECKED(minixCreateNewNodeAt(fs, process, path, mode, &inodenum), unlockTaskLock(&fs->lock));
        unlockTaskLock(&fs->lock);
        return simpleError(SUCCESS);
    } else if (isError(err)) {
        unlockTaskLock(&fs->lock);
        return err;
    } else {
        unlockTaskLock(&fs->lock);
        return simpleError(EEXIST);
    }
}

static Error minixCheckDirectoryCanBeRemoved(VfsFile* file, Process* process, MinixInode* inode) {
    if (inode->size < sizeof(MinixDirEntry)) {
        return simpleError(SUCCESS);
    } else if (inode->size > 2 * sizeof(MinixDirEntry)) {
        return simpleError(EPERM);
    } else {
        size_t tmp_size;
        MinixDirEntry entries[2];
        CHECKED(vfsReadAt(file, process, virtPtrForKernel(entries), inode->size, 0, &tmp_size));
        if (tmp_size != inode->size) {
            return simpleError(EIO);
        } else if (strcmp((char*)entries[0].name, ".") != 0 && strcmp((char*)entries[0].name, "..") != 0) {
            return simpleError(EPERM);
        } else if (
            inode->size == 2 * sizeof(MinixDirEntry)
            && strcmp((char*)entries[1].name, ".") != 0
            && strcmp((char*)entries[1].name, "..") != 0
        ) {
            return simpleError(EPERM);
        } else {
            return simpleError(SUCCESS);
        }
    }
}

static Error minixChangeInodeRefCount(MinixFilesystem* fs, Process* process, uint32_t inodenum, int delta) {
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, inodenum), &tmp_size));
    if (tmp_size != sizeof(MinixInode)) {
        return simpleError(EIO);
    }
    inode.nlinks += delta;
    if (inode.nlinks == 0) {
        VfsFile* file = (VfsFile*)createMinixFileForINode(fs, inodenum, true);
        if (MODE_TYPE(inode.mode) == VFS_TYPE_DIR) {
            CHECKED(minixCheckDirectoryCanBeRemoved(file, process, &inode), file->functions->close(file));
        }
        CHECKED(file->functions->trunc(file, process, 0), file->functions->close(file));
        file->functions->close(file);
        CHECKED(freeMinixInode(fs, inodenum));
        return simpleError(SUCCESS);
    } else {
        CHECKED(vfsWriteAt(fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(fs, inodenum), &tmp_size));
        if (tmp_size != sizeof(MinixInode)) {
            return simpleError(EIO);
        }
        return simpleError(SUCCESS);
    }
}

static Error minixRemoveDirectoryEntry(MinixFilesystem* fs, Process* process, VfsFile* parent, const char* name) {
    uint32_t inodenum;
    size_t offset;
    size_t dir_size;
    CHECKED(minixFindINodeForNameIn((MinixFile*)parent, process, name, &inodenum, &offset, &dir_size));
    CHECKED(minixChangeInodeRefCount(fs, process, inodenum, -1))
    size_t tmp_size;
    MinixDirEntry entry;
    CHECKED(vfsReadAt(parent, process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), dir_size - sizeof(MinixDirEntry), &tmp_size));
    if (tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EIO);
    }
    CHECKED(vfsWriteAt(parent, process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), offset, &tmp_size));
    if (tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EIO);
    }
    CHECKED(parent->functions->trunc(parent, process, dir_size - sizeof(MinixDirEntry)));
    return simpleError(SUCCESS);
}

static Error minixUnlinkFunction(MinixFilesystem* fs, Process* process, const char* path) {
    lockTaskLock(&fs->lock);
    // Try opening the parent
    char* parent_path = getParentPath(path);
    VfsFile* parent;
    CHECKED(fs->base.functions->open((VfsFilesystem*)fs, process, parent_path, VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0, &parent), {
        dealloc(parent_path);
        unlockTaskLock(&fs->lock);
    });
    dealloc(parent_path);
    CHECKED(minixRemoveDirectoryEntry(fs, process, parent, getBaseFilename(path)), {
        parent->functions->close(parent);
        unlockTaskLock(&fs->lock);
    });
    parent->functions->close(parent);
    unlockTaskLock(&fs->lock);
    return simpleError(SUCCESS);
}

static Error minixLinkFunction(MinixFilesystem* fs, Process* process, const char* old, const char* new) {
    lockTaskLock(&fs->lock);
    uint32_t inodenum;
    Error err = minixFindINodeForPath(fs, process, new, &inodenum);
    if (err.kind == ENOENT) {
        CHECKED(minixFindINodeForPath(fs, process, old, &inodenum), unlockTaskLock(&fs->lock));
        // Try opening the parent
        char* parent_path = getParentPath(new);
        VfsFile* parent;
        CHECKED(fs->base.functions->open(
            (VfsFilesystem*)fs, process, parent_path, VFS_OPEN_APPEND | VFS_OPEN_DIRECTORY | VFS_OPEN_WRITE, 0, &parent
        ), {
            dealloc(parent_path);
            unlockTaskLock(&fs->lock);
        });
        dealloc(parent_path);
        CHECKED(minixChangeInodeRefCount(fs, process, inodenum, 1), {
            parent->functions->close(parent);
            unlockTaskLock(&fs->lock);
        });
        CHECKED(minixAppendDirEntryInto(parent, process, inodenum, getBaseFilename(new)), {
            parent->functions->close(parent);
            unlockTaskLock(&fs->lock);
        });
        parent->functions->close(parent);
        unlockTaskLock(&fs->lock);
        return simpleError(SUCCESS);
    } else {
        unlockTaskLock(&fs->lock);
        if (isError(err)) {
            return err;
        } else {
            return simpleError(EEXIST);
        }
    }
}

static Error minixRenameFunction(MinixFilesystem* fs, Process* process, const char* old, const char* new) {
    // Implementation is not ideal, but simple
    CHECKED(minixLinkFunction(fs, process, old, new));
    CHECKED(minixUnlinkFunction(fs, process, old));
    return simpleError(SUCCESS);
}

static void minixFreeFunction(MinixFilesystem* fs) {
    // We don't do any caching. Otherwise write it to disk here.
    fs->block_device->functions->close(fs->block_device);
    dealloc(fs);
}

static Error minixInitFunction(MinixFilesystem* fs, Process* process) {
    size_t tmp_size;
    CHECKED(vfsReadAt(
        fs->block_device, NULL, virtPtrForKernel(&fs->superblock), sizeof(Minix3Superblock),
        MINIX_BLOCK_SIZE, &tmp_size
    ));
    if (tmp_size != sizeof(Minix3Superblock)) {
        return simpleError(EIO);
    } else if (fs->superblock.magic != MINIX3_MAGIC) {
        return simpleError(EPERM);
    } else {
        // Don't do more for now
        return simpleError(SUCCESS);
    }
}

static const VfsFilesystemVtable functions = {
    .open = (OpenFunction)minixOpenFunction,
    .mknod = (MknodFunction)minixMknodFunction,
    .unlink = (UnlinkFunction)minixUnlinkFunction,
    .link = (LinkFunction)minixLinkFunction,
    .rename = (RenameFunction)minixRenameFunction,
    .free = (FreeFunction)minixFreeFunction,
    .init = (InitFunction)minixInitFunction,
};

MinixFilesystem* createMinixFilesystem(VfsFile* block_device, VirtPtr data) {
    MinixFilesystem* file = zalloc(sizeof(MinixFilesystem));
    file->base.functions = &functions;
    file->block_device = block_device;
    return file;
}

