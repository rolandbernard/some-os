
#include "files/minix/node.h"

#include "memory/kalloc.h"

static void minixFree(MinixVfsNode* node) {
}

static Error minixReadAt(MinixVfsNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read) {
}

static Error minixWriteAt(MinixVfsNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written) {
}

static Error minixReaddirAt(MinixVfsNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read) {
}

static Error minixTrunc(MinixVfsNode* node, size_t length) {
}

static Error minixLookup(MinixVfsNode* node, const char* name, size_t* node_id) {
}

static Error minixUnlink(MinixVfsNode* node, const char* name) {
}

static Error minixLink(MinixVfsNode* node, const char* name, MinixVfsNode* entry) {
}

VfsNodeFunctions funcs = {
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
    node->base.stat.dev = fs->base.id;
    node->base.stat.id = inode;
    node->base.stat.rdev = 0;
    node->base.stat.block_size = MINIX_BLOCK_SIZE;
    initTaskLock(&node->base.lock);
    initTaskLock(&node->lock);
    return node;
}

