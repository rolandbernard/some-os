#ifndef _FILES_MINIX_FILE_H_
#define _FILES_MINIX_FILE_H_

#include "files/minix/minix.h"

typedef struct {
    VfsFile base;
    const MinixFilesystem* fs;
    size_t read_position;
    uint32_t inodenum;
} MinixFile;

MinixFile* createMinixFileForINode(const MinixFilesystem* fs, uint32_t inode);

// Special function to read the complete file and return it's contents
MinixDirEntry* readAllFromFile(MinixFile* file);

#endif
