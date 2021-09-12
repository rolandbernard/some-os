#ifndef _FILES_MINIX_H_
#define _FILES_MINIX_H_

#include <stdint.h>

#include "files/vfs.h"
#include "util/spinlock.h"

typedef struct {
    uint32_t ninodes;
    uint16_t pad0;
    uint16_t imap_blocks;
    uint16_t zmap_blocks;
    uint16_t first_data_zone;
    uint16_t log_zone_size;
    uint16_t pad1;
    uint32_t max_size;
    uint32_t zones;
    uint16_t magic;
    uint16_t pag2;
    uint16_t block_size;
    uint8_t disk_version;
} MinixSuperblock;

typedef struct {
    uint16_t mode;
    uint16_t nlinks;
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint32_t zones[10];
} Inode;

typedef struct {
    uint32_t inode;
    uint8_t name[60];
} DirEntry;

typedef struct {
    VfsFilesystem base;
    SpinLock lock;
    MinixSuperblock superblock;
    VfsFile* block_device;
} MinixFilesystem;

MinixFilesystem* createMinixFilesystem(VfsFile* block_device, void* data);

#endif
