#ifndef _FILES_MINIX_FILE_H_
#define _FILES_MINIX_FILE_H_

#include "files/minix/minix.h"

typedef struct {
    VfsFile base;
    MinixFilesystem* fs;
    SpinLock lock;
    size_t position;
    uint32_t inodenum;
} MinixFile;

MinixFile* createMinixFileForINode(MinixFilesystem* fs, uint32_t inode, bool dir);

#endif
