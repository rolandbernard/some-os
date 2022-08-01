#ifndef _FILES_MINIX_MAPS_H_
#define _FILES_MINIX_MAPS_H_

#include "files/minix/minix.h"

Error getFreeMinixInode(MinixFilesystem* fs, uint32_t* inode);

Error getFreeMinixZone(MinixFilesystem* fs, size_t* zone);

Error freeMinixInode(MinixFilesystem* fs, uint32_t inode);

Error freeMinixZone(MinixFilesystem* fs, size_t zone);

#endif
