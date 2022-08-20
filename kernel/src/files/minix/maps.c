
#include "files/minix/maps.h"

#include "files/vfs/file.h"
#include "memory/kalloc.h"
#include "util/util.h"

#define MAX_MAPS_READ_SIZE (1 << 16)

static Error genericMinixGetBitMap(MinixVfsSuperblock* fs, size_t offset, size_t size, bool inode, size_t* pos) {
    lockTaskLock(&fs->maps_lock);
    size_t position = 0;
    size_t bytes = (size + 7) / 8;
    uint8_t* tmp_buffer = kalloc(umin(MAX_MAPS_READ_SIZE, bytes));
    while (bytes > 0) {
        size_t tmp_size = umin(MAX_MAPS_READ_SIZE, bytes);
        CHECKED(vfsFileReadAt(fs->block_device, NULL, virtPtrForKernel(tmp_buffer), tmp_size, offset, &tmp_size), {
            unlockTaskLock(&fs->maps_lock);
            dealloc(tmp_buffer);
        });
        if (tmp_size == 0) {
            unlockTaskLock(&fs->maps_lock);
            dealloc(tmp_buffer);
            return simpleError(EIO);
        }
        for (size_t i = 0; size > 0 && i < tmp_size; i++) {
            uint8_t byte = tmp_buffer[i];
            for (int j = 0; size > 0 && j < 8; j++) {
                if (((byte >> j) & 1) == 0) {
                    // This bit is free
                    tmp_buffer[i] |= (1 << j);
                    CHECKED(vfsFileWriteAt(fs->block_device, NULL, virtPtrForKernel(&tmp_buffer[i]), 1, offset + i, &tmp_size), {
                        unlockTaskLock(&fs->maps_lock);
                        dealloc(tmp_buffer);
                    });
                    if (tmp_size == 0) {
                        unlockTaskLock(&fs->maps_lock);
                        dealloc(tmp_buffer);
                        return simpleError(EIO);
                    }
                    unlockTaskLock(&fs->maps_lock);
                    dealloc(tmp_buffer);
                    *pos = position;
                    return simpleError(SUCCESS);
                }
                size--;
                position++;
            }
        }
        bytes -= tmp_size;
        offset += tmp_size;
    }
    unlockTaskLock(&fs->maps_lock);
    dealloc(tmp_buffer);
    return simpleError(ENOSPC);
}

Error getFreeMinixInode(MinixVfsSuperblock* fs, uint32_t* inode) {
    size_t pos = 0;
    Error res = genericMinixGetBitMap(fs, 2 * MINIX_BLOCK_SIZE, fs->superblock.ninodes, true, &pos);
    *inode = pos;
    return res;
}

Error getFreeMinixZone(MinixVfsSuperblock* fs, size_t* zone) {
    return genericMinixGetBitMap(fs, (2 + fs->superblock.imap_blocks) * MINIX_BLOCK_SIZE, fs->superblock.zones, false, zone);
}

static Error genericMinixClearBitMap(MinixVfsSuperblock* fs, size_t offset, size_t position) {
    lockTaskLock(&fs->maps_lock);
    char tmp_buffer[1];
    size_t size;
    CHECKED(
        vfsFileReadAt(fs->block_device, NULL, virtPtrForKernel(tmp_buffer), 1, offset + position / 8, &size),
        unlockTaskLock(&fs->maps_lock)
    );
    if (size == 0) {
        unlockTaskLock(&fs->maps_lock);
        return simpleError(EIO);
    } else {
        tmp_buffer[0] &= ~(1 << (position % 8));
        CHECKED(
            vfsFileWriteAt(fs->block_device, NULL, virtPtrForKernel(tmp_buffer), 1, offset + position / 8, &size),
            unlockTaskLock(&fs->maps_lock)
        );
        unlockTaskLock(&fs->maps_lock);
        if (size == 0) {
            return simpleError(EIO);
        } else {
            return simpleError(SUCCESS);
        }
    }
}

Error freeMinixInode(MinixVfsSuperblock* fs, uint32_t inode) {
    return genericMinixClearBitMap(fs, 2 * MINIX_BLOCK_SIZE, inode);
}

Error freeMinixZone(MinixVfsSuperblock* fs, size_t zone) {
    return genericMinixClearBitMap(fs, (2 + fs->superblock.imap_blocks) * MINIX_BLOCK_SIZE, zone);
}

