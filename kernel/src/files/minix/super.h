#ifndef _FILES_MINIX_H_
#define _FILES_MINIX_H_

#include "files/minix/types.h"
#include "memory/virtptr.h"

size_t offsetForINode(const MinixVfsSuperblock* sb, uint32_t inode);

size_t offsetForZone(size_t zone);

Error minixWriteNode(MinixVfsSuperblock* sb, MinixVfsNode* write);

Error createMinixVfsSuperblock(VfsFile* block_device, VirtPtr data, VfsSuperblock** ret);

Error registerFsDriverMinix();

#endif
