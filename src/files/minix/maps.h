#ifndef _FILES_MINIX_MAPS_H_
#define _FILES_MINIX_MAPS_H_

#include "files/minix/minix.h"

void getFreeMinixInode(MinixFilesystem* fs, MinixINodeCallback callback, void* udata);

void getFreeMinixZone(MinixFilesystem* fs, VfsFunctionCallbackSizeT callback, void* udata);

void freeMinixInode(MinixFilesystem* fs, uint32_t inode, VfsFunctionCallbackVoid callback, void* udata);

void freeMinixZone(MinixFilesystem* fs, size_t zone, VfsFunctionCallbackVoid callback, void* udata);

#endif
