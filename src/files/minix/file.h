#ifndef _FILES_MINIX_FILE_H_
#define _FILES_MINIX_FILE_H_

#include "files/minix/minix.h"

typedef struct {
    VfsFile base;
    const MinixFilesystem* fs;
    SpinLock lock;
    size_t position;
    uint32_t inodenum;
} MinixFile;

MinixFile* createMinixFileForINode(const MinixFilesystem* fs, uint32_t inode);

void truncateMinixFile(MinixFile* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata);

#endif
