
#include <assert.h>
#include <string.h>

#include "files/minix/node.h"

#include "files/minix/maps.h"
#include "files/minix/super.h"
#include "files/vfs/file.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "util/util.h"

#define SUPER(NODE) ((MinixVfsSuperblock*)NODE->base.superblock)

#define MAX_LOOKUP_READ_SIZE (1 << 16)

static void minixFree(MinixVfsNode* node) {
    dealloc(node);
}

typedef Error (*MinixZoneWalkFunction)(uint32_t* zone, bool* changed, size_t position, size_t size, bool pre, bool post, void* udata);

static Error minixZoneWalkRec(
    MinixVfsNode* node, size_t* position, size_t offset, size_t depth, uint32_t indirect_table, MinixZoneWalkFunction callback, void* udata
);

static Error minixZoneWalkRecScan(
    MinixVfsNode* node, size_t* position, size_t offset, size_t depth, uint32_t* table, size_t len,
    bool* changed, MinixZoneWalkFunction callback, void* udata
) {
    for (size_t i = 0; i < len; i++) {
        size_t size = MINIX_BLOCK_SIZE << (MINIX_NUM_IPTRS_LOG2 * depth);
        if (offset < *position + size) {
            CHECKED(callback(table + i, changed, *position, size, true, false, udata));
            if (depth == 0 || table[i] == 0) {
                CHECKED(callback(table + i, changed, *position, size, false, false, udata));
            } else {
                CHECKED(minixZoneWalkRec(node, position, offset, depth - 1, table[i], callback, udata));
            }
            CHECKED(callback(table + i, changed, *position, size, false, true, udata));
        }
        *position += size;
    }
    return simpleError(SUCCESS);
}

static Error minixZoneWalkRec(
    MinixVfsNode* node, size_t* position, size_t offset, size_t depth, uint32_t indirect_table, MinixZoneWalkFunction callback, void* udata
) {
    uint32_t* table = kalloc(MINIX_BLOCK_SIZE);
    size_t tmp_size;
    CHECKED(vfsFileReadAt(
        SUPER(node)->block_device, NULL, virtPtrForKernel(table), offsetForZone(indirect_table), MINIX_BLOCK_SIZE, &tmp_size
    ), dealloc(table));
    if (tmp_size != MINIX_BLOCK_SIZE) {
        dealloc(table);
        return simpleError(EIO);
    }
    bool changed = false;
    Error err = minixZoneWalkRecScan(node, position, offset, depth, table, MINIX_NUM_IPTRS, &changed, callback, udata);
    if (changed) {
        Error e = vfsFileWriteAt(
            SUPER(node)->block_device, NULL, virtPtrForKernel(table), offsetForZone(indirect_table), MINIX_BLOCK_SIZE, &tmp_size
        );
        if (!isError(err)) {
            err = e;
        }
    }
    dealloc(table);
    return err;
}

static Error minixZoneWalk(
    MinixVfsNode* node, size_t offset, MinixZoneWalkFunction callback, void* udata
) {
    bool changed = false;
    size_t position = 0;
    Error err = minixZoneWalkRecScan(node, &position, offset, 0, node->zones, 7, &changed, callback, udata);
    if (!isError(err)) {
        err = minixZoneWalkRecScan(node, &position, offset, 1, node->zones + 7, 1, &changed, callback, udata);
    }
    if (!isError(err)) {
        err = minixZoneWalkRecScan(node, &position, offset, 2, node->zones + 8, 1, &changed, callback, udata);
    }
    if (!isError(err)) {
        err = minixZoneWalkRecScan(node, &position, offset, 3, node->zones + 9, 1, &changed, callback, udata);
    }
    if (changed) {
        Error e = minixWriteNode(SUPER(node), node);
        if (!isError(err)) {
            err = e;
        }
    }
    return err;
}

typedef struct {
    MinixVfsNode* node;
    VirtPtr buffer;
    size_t offset;
    size_t left;
    bool write;
} MinixReadWriteRequest;

static Error minixRWZoneWalkCallback(uint32_t* zone, bool* changed, size_t position, size_t size, bool pre, bool post, void* udata) {
    MinixReadWriteRequest* request = (MinixReadWriteRequest*)udata;
    if (request->write && pre) {
        if (*zone == 0 && request->left > 0) {
            size_t new_zone;
            CHECKED(getFreeMinixZone(SUPER(request->node), &new_zone));
            assert(MINIX_BLOCK_SIZE < PAGE_SIZE);
            size_t tmp_size;
            CHECKED(vfsFileWriteAt(
                SUPER(request->node)->block_device, NULL, virtPtrForKernel(zero_page), offsetForZone(new_zone), MINIX_BLOCK_SIZE, &tmp_size
            ));
            if (tmp_size != MINIX_BLOCK_SIZE) {
                return simpleError(EIO);
            }
            *zone = new_zone;
            *changed = true;
        }
    }
    if (!pre && !post && request->left > 0) {
        size_t file_offset = request->offset > position ? request->offset : position;
        size_t block_offset = file_offset % MINIX_BLOCK_SIZE;
        size_t tmp_size = umin(request->left, position + size - file_offset);
        if (request->write) {
            CHECKED(vfsFileWriteAt(
                SUPER(request->node)->block_device, NULL, request->buffer, offsetForZone(*zone) + block_offset, tmp_size, &tmp_size
            ));
        } else if (*zone == 0) {
            memsetVirtPtr(request->buffer, 0, tmp_size);
        } else {
            CHECKED(vfsFileReadAt(
                SUPER(request->node)->block_device, NULL, request->buffer, offsetForZone(*zone) + block_offset, tmp_size, &tmp_size
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

static Error minixReadWrite(MinixVfsNode* node, VirtPtr buffer, size_t offset, size_t length, bool write, size_t* ret) {
    lockTaskLock(&node->lock);
    if (!write) {
        if (node->base.stat.size < offset) {
            length = 0;
        } else {
            length = umin(length, node->base.stat.size - offset);
        }
    }
    if (length == 0) {
        unlockTaskLock(&node->lock);
        *ret = 0;
        return simpleError(SUCCESS);
    }
    MinixReadWriteRequest request = {
        .node = node,
        .buffer = buffer,
        .offset = offset,
        .left = length,
        .write = write,
    };
    Error err = minixZoneWalk(node, offset, minixRWZoneWalkCallback, &request);
    if (err.kind != SUCCESS_EXIT && isError(err)) {
        unlockTaskLock(&node->lock);
        return err;
    } else {
        length -= request.left;
        if (write && offset + length > node->base.stat.size) {
            lockTaskLock(&node->base.lock);
            node->base.stat.size = offset + length;
            minixWriteNode(SUPER(node), node);
            unlockTaskLock(&node->base.lock);
        }
        unlockTaskLock(&node->lock);
        *ret = length;
        return simpleError(SUCCESS);
    }
}

static Error minixReadAt(MinixVfsNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    return minixReadWrite(node, buff, offset, length, false, read);
}

static Error minixWriteAt(MinixVfsNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written) {
    return minixReadWrite(node, buff, offset, length, true, written);
}

static Error minixReaddirAt(MinixVfsNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read_file, size_t* written_buff) {
    size_t tmp_size;
    MinixDirEntry entry;
    CHECKED(minixReadAt(node, virtPtrForKernel(&entry), offset, sizeof(MinixDirEntry), &tmp_size));
    if (tmp_size != 0 && tmp_size != sizeof(MinixDirEntry)) {
        return simpleError(EIO);
    } else {
        *read_file = tmp_size;
        if (tmp_size == 0) {
            *written_buff = 0;
            return simpleError(SUCCESS);
        } else {
            size_t name_len = strlen((char*)entry.name);
            size_t vfs_entry_size = sizeof(VfsDirectoryEntry) + name_len + 1;
            VfsDirectoryEntry* vfs_entry = kalloc(vfs_entry_size);
            vfs_entry->id = entry.inode;
            vfs_entry->off = offset;
            vfs_entry->len = vfs_entry_size;
            vfs_entry->type = VFS_TYPE_UNKNOWN;
            memcpy(vfs_entry->name, entry.name, name_len + 1);
            memcpyBetweenVirtPtr(buff, virtPtrForKernel(vfs_entry), umin(length, vfs_entry_size));
            *written_buff = umin(vfs_entry_size, length);
            dealloc(vfs_entry);
            return simpleError(SUCCESS);
        }
    }
}

typedef struct {
    MinixVfsNode* file;
    size_t length;
} MinixTruncRequest;

static Error minixTruncZoneWalkCallback(uint32_t* zone, bool* changed, size_t position, size_t size, bool pre, bool post, void* udata) {
    MinixTruncRequest* request = (MinixTruncRequest*)udata;
    if (*zone != 0) {
        if (request->length < position + size) {
            if (!pre && !post) { // Zero the rest of the zone
                size_t block_offset = request->length % position;
                size_t tmp_size = MINIX_BLOCK_SIZE - block_offset;
                assert(MINIX_BLOCK_SIZE < PAGE_SIZE);
                CHECKED(vfsFileWriteAt(
                    SUPER(request->file)->block_device, NULL, virtPtrForKernel(zero_page),
                    offsetForZone(*zone) + block_offset, tmp_size, &tmp_size
                ));
            }
        } else {
            if (post) { // Free the zone in the filesystem zone map
                CHECKED(freeMinixZone(SUPER(request->file), *zone));
                *zone = 0;
                *changed = true;
            }
        }
    }
    return simpleError(SUCCESS);
}

static Error minixTrunc(MinixVfsNode* node, size_t length) {
    lockTaskLock(&node->lock);
    MinixTruncRequest request = {
        .file = node,
        .length = length,
    };
    Error err = minixZoneWalk(node, length, minixTruncZoneWalkCallback, &request);
    if (err.kind != SUCCESS_EXIT && isError(err)) {
        unlockTaskLock(&node->lock);
        return err;
    } else {
        lockTaskLock(&node->base.lock);
        node->base.stat.size = length;
        minixWriteNode(SUPER(node), node);
        unlockTaskLock(&node->base.lock);
        unlockTaskLock(&node->lock);
        return simpleError(SUCCESS);
    }
}

static Error minixInternalLookup(MinixVfsNode* node, const char* name, uint32_t* inodenum, size_t* off) {
    size_t offset = 0;
    size_t left = node->base.stat.size;
    MinixDirEntry* tmp_buffer = kalloc(umin(MAX_LOOKUP_READ_SIZE, left));
    while (left > 0) {
        size_t tmp_size = umin(MAX_LOOKUP_READ_SIZE, left);
        CHECKED(minixReadAt(node, virtPtrForKernel(tmp_buffer), offset, tmp_size, &tmp_size), {
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

static Error minixLookup(MinixVfsNode* node, const char* name, size_t* node_id) {
    uint32_t inodenum;
    size_t offset;
    lockTaskLock(&node->lock);
    Error err = minixInternalLookup(node, name, &inodenum, &offset);
    unlockTaskLock(&node->lock);
    *node_id = inodenum;
    return err;
}

static Error minixUnlink(MinixVfsNode* node, const char* name) {
    uint32_t inodenum;
    size_t offset;
    lockTaskLock(&node->lock);
    CHECKED(minixInternalLookup(node, name, &inodenum, &offset), unlockTaskLock(&node->lock));
    MinixDirEntry entry;
    size_t tmp_size;
    CHECKED(
        minixReadAt(node, virtPtrForKernel(&entry), node->base.stat.size - sizeof(MinixDirEntry), sizeof(MinixDirEntry), &tmp_size),
        unlockTaskLock(&node->lock)
    );
    if (tmp_size != sizeof(MinixDirEntry)) {
        unlockTaskLock(&node->lock);
        return simpleError(EIO);
    }
    CHECKED(minixWriteAt(node, virtPtrForKernel(&entry), offset, sizeof(MinixDirEntry), &tmp_size), unlockTaskLock(&node->lock));
    Error err = minixTrunc(node, node->base.stat.size - sizeof(MinixDirEntry));
    unlockTaskLock(&node->lock);
    return err;
}

static Error minixLink(MinixVfsNode* node, const char* name, MinixVfsNode* entry_node) {
    MinixDirEntry entry = { .inode = entry_node->base.stat.id };
    size_t name_len = strlen(name);
    if (name_len >= sizeof(entry.name)) {
        name_len = sizeof(entry.name) - 1;
    }
    memcpy(entry.name, name, name_len);
    entry.name[name_len] = 0;
    size_t tmp_size;
    lockTaskLock(&node->lock);
    CHECKED(
        minixWriteAt(node, virtPtrForKernel(&entry), node->base.stat.size, sizeof(MinixDirEntry), &tmp_size),
        unlockTaskLock(&node->lock)
    );
    unlockTaskLock(&node->lock);
    return simpleError(tmp_size != sizeof(MinixDirEntry) ? EIO : SUCCESS);
}

static const VfsNodeFunctions funcs = {
    .free = (VfsNodeFreeFunction)minixFree,
    .read_at = (VfsNodeReadAtFunction)minixReadAt,
    .write_at = (VfsNodeWriteAtFunction)minixWriteAt,
    .trunc = (VfsNodeTruncFunction)minixTrunc,
    .readdir_at = (VfsNodeReaddirAtFunction)minixReaddirAt,
    .lookup = (VfsNodeLookupFunction)minixLookup,
    .unlink = (VfsNodeUnlinkFunction)minixUnlink,
    .link = (VfsNodeLinkFunction)minixLink,
};

MinixVfsNode* createMinixVfsNode(MinixVfsSuperblock* fs, uint32_t inode) {
    MinixVfsNode* node = kalloc(sizeof(MinixVfsNode));
    node->base.superblock = (VfsSuperblock*)fs;
    node->base.functions = &funcs;
    node->base.mounted = NULL;
    node->base.ref_count = 0;
    node->base.real_node = (VfsNode*)node;
    node->base.stat.dev = fs->base.id;
    node->base.stat.id = inode;
    node->base.stat.rdev = 0;
    node->base.stat.block_size = MINIX_BLOCK_SIZE;
    initTaskLock(&node->base.lock);
    initTaskLock(&node->lock);
    return node;
}

