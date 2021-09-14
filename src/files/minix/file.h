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

typedef bool (*MinixReadDirectoryCallback)(Error error, MinixDirEntry* entry, void* udata);

// Utility function to read the complete file and streams it's contents using the callback.
// If the file is not a directory or at the end of the directory it is called with NULL.
// The stream will halt if the callback returns false.
void readAllFromDirectory(MinixFile* file, MinixReadDirectoryCallback callback, void* udata);

#endif
