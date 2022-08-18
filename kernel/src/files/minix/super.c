
#include "files/minix/super.h"

size_t offsetForINode(const Minix3Superblock* sb, uint32_t inode) {
    return (2 + sb->imap_blocks + sb->zmap_blocks) * MINIX_BLOCK_SIZE
           + (inode - 1) * sizeof(MinixInode);
}

size_t offsetForZone(size_t zone) {
    return zone * MINIX_BLOCK_SIZE;
}

#define MAX_SINGLE_READ_SIZE (1 << 16)

// TODO

Error createMinixVfsSuperblock(VfsFile* block_device, VirtPtr data, MinixVfsSuperblock** ret) {
    return simpleError(SUCCESS);
}

