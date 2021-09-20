#ifndef _FILES_MINIX_MAPS_H_
#define _FILES_MINIX_MAPS_H_

#include "files/minix/minix.h"

void getFreeMinixInode(MinixFilesystem* fs, Uid uid, Gid gid, MinixINodeCallback callback, void* udata);

void getFreeMinixZone(MinixFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackSizeT callback, void* udata);

void freeMinixInode(MinixFilesystem* fs, Uid uid, Gid gid, uint32_t inode, VfsFunctionCallbackVoid callback, void* udata);

void freeMinixZone(MinixFilesystem* fs, Uid uid, Gid gid, size_t zone, VfsFunctionCallbackVoid callback, void* udata);

#endif
