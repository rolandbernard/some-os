
#include <assert.h>
#include <string.h>

#include "files/minix/file.h"

#include "files/minix/maps.h"
#include "error/log.h"
#include "error/panic.h"
#include "files/vfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "util/util.h"

typedef Error (*MinixZoneWalkFunction)(uint32_t* zone, size_t position, size_t size, bool pre, bool post, void* udata);

static Error minixZoneWalkRec(
    MinixFile* file, size_t* position, size_t offset, size_t depth, uint32_t indirect_table, MinixZoneWalkFunction callback, void* udata
);

static Error minixZoneWalkRecScan(
    MinixFile* file, size_t* position, size_t offset, size_t depth, uint32_t* table, size_t len, MinixZoneWalkFunction callback, void* udata
) {
    for (size_t i = 0; i < len; i++) {
        size_t size = MINIX_BLOCK_SIZE << (MINIX_NUM_IPTRS_LOG2 * depth);
        if (offset < *position + size) {
            CHECKED(callback(table + i, *position, size, true, false, udata));
            if (depth == 0 || table[i] == 0) {
                CHECKED(callback(table + i, *position, size, false, false, udata));
            } else {
                CHECKED(minixZoneWalkRec(file, position, offset, depth - 1, table[i], callback, udata));
            }
            CHECKED(callback(table + i, *position, size, false, true, udata));
        }
        *position += size;
    }
    return simpleError(SUCCESS);
}

static Error minixZoneWalkRec(
    MinixFile* file, size_t* position, size_t offset, size_t depth, uint32_t indirect_table, MinixZoneWalkFunction callback, void* udata
) {
    uint32_t* table = kalloc(MINIX_BLOCK_SIZE);
    size_t tmp_size;
    CHECKED(vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(table), MINIX_BLOCK_SIZE, offsetForZone(indirect_table), &tmp_size
    ), dealloc(table));
    if (tmp_size != MINIX_BLOCK_SIZE) {
        dealloc(table);
        return simpleError(EIO);
    }
    CHECKED(minixZoneWalkRecScan(file, position, offset, depth, table, MINIX_NUM_IPTRS, callback, udata), dealloc(table));
    dealloc(table);
    return simpleError(SUCCESS);
}

static Error minixZoneWalk(
    MinixFile* file, size_t offset, MinixInode* inode, MinixZoneWalkFunction callback, void* udata
) {
    size_t position = 0;
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 0, inode->zones, 7, callback, udata));
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 1, inode->zones + 7, 1, callback, udata));
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 2, inode->zones + 8, 1, callback, udata));
    CHECKED(minixZoneWalkRecScan(file, &position, offset, 3, inode->zones + 9, 1, callback, udata));
    return simpleError(SUCCESS);
}

typedef struct {
    MinixFile* file;
    Process* process;
    VirtPtr buffer;
    size_t length;
    size_t left;
    bool write;
} MinixReadWriteRequest;

static Error minixRWZoneWalkCallback(uint32_t* zone, size_t position, size_t size, bool pre, bool post, void* udata) {
    MinixReadWriteRequest* request = (MinixReadWriteRequest*)udata;
    if (request->write && pre) {
        if (*zone == 0 && request->left > 0) {
            size_t new_zone;
            CHECKED(getFreeMinixZone(request->file->fs, &new_zone));
            assert(MINIX_BLOCK_SIZE < PAGE_SIZE);
            size_t tmp_size;
            CHECKED(vfsWriteAt(
                request->file->fs->block_device, NULL, virtPtrForKernel(zero_page), MINIX_BLOCK_SIZE, offsetForZone(new_zone), &tmp_size
            ));
            if (tmp_size != MINIX_BLOCK_SIZE) {
                return simpleError(EIO);
            }
            *zone = new_zone;
        }
    }
    if (!pre && !post && request->left > 0) {
        size_t file_offset =
            request->file->position > position ? request->file->position : position;
        size_t block_offset = file_offset % MINIX_BLOCK_SIZE;
        size_t tmp_size = umin(request->left, position + size - file_offset);
        if (request->write) {
            CHECKED(vfsWriteAt(
                request->file->fs->block_device, NULL, request->buffer, tmp_size, offsetForZone(*zone) + block_offset, &tmp_size
            ));
        } else if (*zone == 0) {
            memsetVirtPtr(request->buffer, 0, tmp_size);
        } else {
            CHECKED(vfsReadAt(
                request->file->fs->block_device, NULL, request->buffer, tmp_size, offsetForZone(*zone) + block_offset, &tmp_size
            ));
        }
        if (tmp_size == 0) {
            return simpleError(EIO);
        }
        request->buffer.address += tmp_size;
        request->left -= tmp_size;
    }
    if (request->left == 0) {
        return simpleError(SUCCESS_EXIT);
    } else {
        return simpleError(SUCCESS);
    }
}

static Error minixReadWriteFunction(MinixFile* file, Process* process, VirtPtr buffer, size_t length, bool write, size_t* ret) {
    lockTaskLock(&file->lock);
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(file->fs, file->inodenum), &tmp_size
    ), {
        unlockTaskLock(&file->lock);
    });
    if (tmp_size != sizeof(MinixInode)) {
        unlockTaskLock(&file->lock);
        return simpleError(EIO);
    } else if (!canAccess(inode.mode, inode.uid, inode.gid, process, write ? VFS_ACCESS_W : VFS_ACCESS_R)) {
        unlockTaskLock(&file->lock);
        return simpleError(EACCES);
    }
    if (!write) {
        if (inode.size < file->position) {
            length = 0;
        } else {
            length = umin(length, inode.size - file->position);
        }
    }
    if (length == 0) {
        *ret = 0;
        unlockTaskLock(&file->lock);
        return simpleError(SUCCESS);
    }
    MinixReadWriteRequest request = {
        .file = file,
        .process = process,
        .buffer = buffer,
        .length = length,
        .left = length,
        .write = write,
    };
    Error err = minixZoneWalk(file, file->position, &inode, minixRWZoneWalkCallback, &request);
    if (err.kind != SUCCESS_EXIT && isError(err)) {
        unlockTaskLock(&file->lock);
        return err;
    } else {
        *ret = length;
        file->position += length;
        if (write) {
            if (file->position > inode.size) {
                inode.size = file->position;
            }
            inode.mtime = getUnixTime();
        }
        inode.atime = getUnixTime();
        CHECKED(vfsWriteAt(
            file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(file->fs, file->inodenum), &tmp_size
        ), {
            unlockTaskLock(&file->lock);
        });
        unlockTaskLock(&file->lock);
        return simpleError(SUCCESS);
    }
}

static Error minixReadFunction(MinixFile* file, Process* process, VirtPtr buffer, size_t length, size_t* ret) {
    return minixReadWriteFunction(file, process, buffer, length, false, ret);
}

static Error minixWriteFunction(MinixFile* file, Process* process, VirtPtr buffer, size_t length, size_t* ret) {
    return minixReadWriteFunction(file, process, buffer, length, true, ret);
}

static Error minixSeekFunction(MinixFile* file, Process* process, size_t offset, VfsSeekWhence whence, size_t* ret) {
    lockTaskLock(&file->lock);
    size_t new_position;
    if (whence == VFS_SEEK_END) { // We have to read the size to know where the end is
        size_t tmp_size;
        MinixInode inode;
        CHECKED(vfsReadAt(
            file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(file->fs, file->inodenum), &tmp_size
        ), {
            unlockTaskLock(&file->lock);
        });
        if (tmp_size != sizeof(MinixInode)) {
            unlockTaskLock(&file->lock);
            return simpleError(EIO);
        }
        new_position = inode.size + offset;
    } else {
        switch (whence) {
            case VFS_SEEK_CUR:
                new_position = file->position + offset;
                break;
            case VFS_SEEK_SET:
                new_position = offset;
                break;
            default:
                panic();
        }
    }
    file->position = new_position;
    *ret = new_position;
    unlockTaskLock(&file->lock);
    return simpleError(SUCCESS);
}

static Error minixStatFunction(MinixFile* file, Process* process, VirtPtr stat) {
    lockTaskLock(&file->lock);
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(file->fs, file->inodenum), &tmp_size
    ), {
        unlockTaskLock(&file->lock);
    });
    unlockTaskLock(&file->lock);
    if (tmp_size != sizeof(MinixInode)) {
        return simpleError(EIO);
    }
    VfsStat ret = {
        .id = file->inodenum,
        .mode = inode.mode,
        .nlinks = inode.nlinks,
        .uid = inode.uid,
        .gid = inode.gid,
        .size = inode.size,
        .block_size = 1,
        .st_atime = inode.atime * 1000000000UL,
        .st_mtime = inode.mtime * 1000000000UL,
        .st_ctime = inode.ctime * 1000000000UL,
        .dev = file->fs->block_device->ino,
    };
    memcpyBetweenVirtPtr(stat, virtPtrForKernel(&ret), sizeof(VfsStat));
    return simpleError(SUCCESS);
}

static void minixCloseFunction(MinixFile* file, Process* process) {
    lockTaskLock(&file->lock);
    lockTaskLock(&file->fs->lock);
    file->fs->base.open_files--;
    unlockTaskLock(&file->fs->lock);
    dealloc(file);
}

static Error minixDupFunction(MinixFile* file, Process* process, VfsFile** ret) {
    MinixFile* copy = kalloc(sizeof(MinixFile));
    lockTaskLock(&file->lock);
    lockTaskLock(&file->fs->lock);
    file->fs->base.open_files++;
    unlockTaskLock(&file->fs->lock);
    memcpy(copy, file, sizeof(MinixFile));
    unlockTaskLock(&file->lock);
    unlockTaskLock(&copy->lock); // Also unlock the new file
    *ret = (VfsFile*)copy;
    return simpleError(SUCCESS);
}

typedef struct {
    MinixFile* file;
    Process* process;
    size_t length;
} MinixTruncRequest;

static Error minixTruncZoneWalkCallback(uint32_t* zone, size_t position, size_t size, bool pre, bool post, void* udata) {
    MinixTruncRequest* request = (MinixTruncRequest*)udata;
    if (*zone != 0) {
        if (request->length < position + size) {
            if (!pre && !post) { // Zero the rest of the zone
                size_t block_offset = request->length % position;
                size_t tmp_size = MINIX_BLOCK_SIZE - block_offset;
                assert(MINIX_BLOCK_SIZE < PAGE_SIZE);
                CHECKED(vfsWriteAt(
                    request->file->fs->block_device, NULL, virtPtrForKernel(zero_page), tmp_size, offsetForZone(*zone) + block_offset, &tmp_size
                ));
            }
        } else {
            if (post) { // Free the zone in the filesystem zone map
                CHECKED(freeMinixZone(request->file->fs, *zone));
                *zone = 0;
            }
        }
    }
    return simpleError(SUCCESS);
}

static Error minixTruncFunction(MinixFile* file, Process* process, size_t size) {
    lockTaskLock(&file->lock);
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode),
        offsetForINode(file->fs, file->inodenum), &tmp_size
    ), {
        unlockTaskLock(&file->lock);
    });
    if (tmp_size != sizeof(MinixInode)) {
        unlockTaskLock(&file->lock);
        return simpleError(EIO);
    } else if (!canAccess(inode.mode, inode.uid, inode.gid, process, VFS_ACCESS_W)) {
        unlockTaskLock(&file->lock);
        return simpleError(EACCES);
    }
    MinixTruncRequest request = {
        .file = file,
        .process = process,
        .length = umin(size, inode.size),
    };
    Error err = minixZoneWalk(file, file->position, &inode, minixTruncZoneWalkCallback, &request);
    if (err.kind != SUCCESS_EXIT && isError(err)) {
        unlockTaskLock(&file->lock);
        return err;
    } else {
        inode.size = size;
        inode.mtime = getUnixTime();
        inode.atime = getUnixTime();
        CHECKED(vfsWriteAt(
            file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(file->fs, file->inodenum), &tmp_size
        ), {
            unlockTaskLock(&file->lock);
        });
        unlockTaskLock(&file->lock);
        return simpleError(SUCCESS);
    }
}

static Error minixChFunction(MinixFile* file, Process* process, VfsMode new_mode, Uid new_uid, Gid new_gid, bool chown) {
    lockTaskLock(&file->lock);
    size_t tmp_size;
    MinixInode inode;
    CHECKED(vfsReadAt(
        file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(file->fs, file->inodenum), &tmp_size
    ), {
        unlockTaskLock(&file->lock);
    });
    if (tmp_size != sizeof(MinixInode)) {
        unlockTaskLock(&file->lock);
        return simpleError(EIO);
    } else if (process != NULL && process->resources.uid != 0 && process->resources.uid != inode.uid) {
        unlockTaskLock(&file->lock);
        return simpleError(EACCES);
    }
    if (chown) {
        inode.uid = new_uid;
        inode.gid = new_gid;
    } else {
        // Don't allow changing the type
        inode.mode &= VFS_MODE_TYPE; // Clear everything but the type
        inode.mode |= new_mode & ~VFS_MODE_TYPE; 
    }
    CHECKED(vfsWriteAt(
        file->fs->block_device, NULL, virtPtrForKernel(&inode), sizeof(MinixInode), offsetForINode(file->fs, file->inodenum), &tmp_size
    ), {
        unlockTaskLock(&file->lock);
    });
    unlockTaskLock(&file->lock);
    if (tmp_size != sizeof(MinixInode)) {
        return simpleError(EIO);
    }
    return simpleError(SUCCESS);
}

static Error minixChmodFunction(MinixFile* file, Process* process, VfsMode mode) {
    return minixChFunction(file, process, mode, 0, 0, false);
}

static Error minixChownFunction(MinixFile* file, Process* process, Uid new_uid, Gid new_gid) {
    return minixChFunction(file, process, 0, new_uid, new_gid, true);
}

static Error minixReaddirFunction(MinixFile* file, Process* process, VirtPtr buff, size_t size, size_t* ret) {
    size_t position = file->position;
    size_t tmp_size;
    MinixDirEntry entry;
    CHECKED(file->base.functions->read((VfsFile*)file, process, virtPtrForKernel(&entry), sizeof(MinixDirEntry), &tmp_size));
    if (tmp_size != 0 && tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EACCES);
    } else {
        if (tmp_size == 0) {
            *ret = 0;
            return simpleError(SUCCESS);
        } else {
            size_t name_len = strlen((char*)entry.name);
            size_t vfs_entry_size = sizeof(VfsDirectoryEntry) + name_len + 1;
            VfsDirectoryEntry* vfs_entry = kalloc(vfs_entry_size);
            vfs_entry->id = entry.inode;
            vfs_entry->off = position;
            vfs_entry->len = vfs_entry_size;
            vfs_entry->type = VFS_TYPE_UNKNOWN;
            memcpy(vfs_entry->name, entry.name, name_len + 1);
            memcpyBetweenVirtPtr(buff, virtPtrForKernel(vfs_entry), umin(size, vfs_entry_size));
            dealloc(vfs_entry);
            *ret = umin(size, vfs_entry_size);
            return simpleError(SUCCESS);
        }
    }
}

static VfsFileVtable functions_file = {
    .seek = (SeekFunction)minixSeekFunction,
    .read = (ReadFunction)minixReadFunction,
    .write = (WriteFunction)minixWriteFunction,
    .stat = (StatFunction)minixStatFunction,
    .close = (CloseFunction)minixCloseFunction,
    .dup = (DupFunction)minixDupFunction,
    .trunc = (TruncFunction)minixTruncFunction,
    .chmod = (ChmodFunction)minixChmodFunction,
    .chown = (ChownFunction)minixChownFunction,
};

static VfsFileVtable functions_directory = {
    .seek = (SeekFunction)minixSeekFunction,
    .read = (ReadFunction)minixReadFunction,
    .write = (WriteFunction)minixWriteFunction,
    .stat = (StatFunction)minixStatFunction,
    .close = (CloseFunction)minixCloseFunction,
    .dup = (DupFunction)minixDupFunction,
    .trunc = (TruncFunction)minixTruncFunction,
    .chmod = (ChmodFunction)minixChmodFunction,
    .chown = (ChownFunction)minixChownFunction,
    .readdir = (ReaddirFunction)minixReaddirFunction,
};

MinixFile* createMinixFileForINode(MinixFilesystem* fs, uint32_t inode, bool dir) {
    fs->base.open_files++;
    MinixFile* file = zalloc(sizeof(MinixFile));
    if (dir) {
        file->base.functions = &functions_directory;
    } else {
        file->base.functions = &functions_file;
    }
    file->fs = fs;
    file->inodenum = inode;
    file->position = 0;
    return file;
}

