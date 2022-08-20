#ifndef _FILES_MINIX_FILE_H_
#define _FILES_MINIX_FILE_H_

#include "files/minix/types.h"

MinixVfsNode* createMinixVfsNode(MinixVfsSuperblock* fs, uint32_t inode);

#endif
