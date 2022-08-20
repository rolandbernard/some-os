
#include <assert.h>
#include <string.h>

#include "files/minix/super.h"

#include "files/minix/node.h"
#include "files/minix/maps.h"
#include "files/vfs/cache.h"
#include "files/vfs/file.h"
#include "memory/kalloc.h"

size_t offsetForINode(const MinixVfsSuperblock* sb, uint32_t inode) {
    return (2 + sb->superblock.imap_blocks + sb->superblock.zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

size_t offsetForZone(size_t zone) {
    return zone * MINIX_BLOCK_SIZE;
}

#define MAX_SINGLE_READ_SIZE (1 << 16)

static void minixFreeSuperblock(MinixVfsSuperblock* sb) {
    vfsFileClose(sb->block_device);
    dealloc(sb);
}

static Error minixReadNode(MinixVfsSuperblock* sb, size_t id, MinixVfsNode** out) {
    MinixVfsNode* node = createMinixVfsNode(sb, id);
    MinixInode inode;
    size_t tmp_size;
    CHECKED(
        vfsFileReadAt(sb->block_device, NULL, virtPtrForKernel(&inode), offsetForINode(sb, id), sizeof(MinixInode), &tmp_size),
        dealloc(node)
    );
    if (tmp_size != sizeof(MinixInode)) {
        dealloc(node);
        return simpleError(EIO);
    }
    node->base.stat.mode = inode.mode;
    node->base.stat.nlinks = inode.nlinks;
    node->base.stat.uid = inode.uid;
    node->base.stat.gid = inode.gid;
    node->base.stat.size = inode.size;
    node->base.stat.atime = inode.atime * 1000000000UL;
    node->base.stat.mtime = inode.mtime * 1000000000UL;
    node->base.stat.ctime = inode.ctime * 1000000000UL;
    node->base.stat.blocks = 0; // We don't support this for now.
    memcpy(node->zones, inode.zones, sizeof(inode.zones));
    *out = node;
    return simpleError(SUCCESS);
}

Error minixWriteNode(MinixVfsSuperblock* sb, MinixVfsNode* write) {
    lockTaskLock(&write->base.lock);
    MinixInode inode = {
        .mode = write->base.stat.mode,
        .nlinks = write->base.stat.nlinks,
        .uid = write->base.stat.uid,
        .gid = write->base.stat.gid,
        .size = write->base.stat.size,
        .atime = write->base.stat.atime,
        .mtime = write->base.stat.mtime,
        .ctime = write->base.stat.ctime,
    };
    unlockTaskLock(&write->base.lock);
    lockTaskLock(&write->lock);
    memcpy(inode.zones, write->zones, sizeof(inode.zones));
    size_t tmp_size;
    CHECKED(
        vfsFileWriteAt(sb->block_device, NULL, virtPtrForKernel(&inode), offsetForINode(sb, write->base.stat.id), sizeof(MinixInode), &tmp_size), 
        unlockTaskLock(&write->lock);
    );
    unlockTaskLock(&write->lock);
    return simpleError(tmp_size != sizeof(MinixInode) ? EIO : SUCCESS);
}

static Error minixNewNode(MinixVfsSuperblock* sb, size_t* id) {
    uint32_t inodenum;
    Error err = getFreeMinixInode(sb, &inodenum);
    *id = inodenum;
    return err;
}

static Error minixFreeNode(MinixVfsSuperblock* sb, MinixVfsNode* free) {
    for (size_t i = 0; i < 10; i++) {
        assert(free->zones[i] = 0); // All zones should have been freed already.
    }
    return freeMinixInode(sb, free->base.stat.id);
}

VfsSuperblockFunctions funcs = {
    .free = (VfsSuperblockFreeFunction)minixFreeSuperblock,
    .read_node = (VfsSuperblockReadNodeFunction)minixReadNode,
    .write_node = (VfsSuperblockWriteNodeFunction)minixWriteNode,
    .new_node = (VfsSuperblockNewNodeFunction)minixNewNode,
    .free_node = (VfsSuperblockFreeNodeFunction)minixFreeNode,
};

Error createMinixVfsSuperblock(VfsFile* block_device, VirtPtr data, MinixVfsSuperblock** ret) {
    MinixVfsSuperblock* sb = kalloc(sizeof(MinixVfsSuperblock));
    size_t tmp_size;
    CHECKED(
        vfsFileReadAt(block_device, NULL, virtPtrForKernel(&sb->superblock), MINIX_BLOCK_SIZE, sizeof(Minix3Superblock), &tmp_size),
        dealloc(sb)
    );
    if (tmp_size != sizeof(Minix3Superblock)) {
        dealloc(sb);
        return simpleError(EIO);
    } else if (sb->superblock.magic != MINIX3_MAGIC) {
        dealloc(sb);
        return simpleError(EINVAL);
    } else {
        sb->base.id = block_device->node->stat.rdev;
        sb->base.ref_count = 1;
        sb->base.functions = &funcs;
        sb->block_device = block_device;
        initTaskLock(&sb->base.lock);
        initTaskLock(&sb->maps_lock);
        CHECKED(minixReadNode(sb, 1, (MinixVfsNode**)&sb->base.root_node), dealloc(sb));
        vfsCacheInit(&sb->base.nodes);
        vfsFileCopy(block_device);
        return simpleError(SUCCESS);
    }
}

