#ifndef _FILES_MINIX_MAPS_H_
#define _FILES_MINIX_MAPS_H_

#include "files/minix/super.h"

Error getFreeMinixInode(MinixVfsSuperblock* fs, uint32_t* inode);

Error getFreeMinixZone(MinixVfsSuperblock* fs, size_t* zone);

Error freeMinixInode(MinixVfsSuperblock* fs, uint32_t inode);

Error freeMinixZone(MinixVfsSuperblock* fs, size_t zone);

#endif
