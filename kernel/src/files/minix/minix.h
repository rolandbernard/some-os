#ifndef _FILES_MINIX_H_
#define _FILES_MINIX_H_

#include <stdint.h>

#include "files/vfs.h"
#include "memory/virtptr.h"
#include "task/spinlock.h"

#define MINIX3_MAGIC 0x4d5a
#define MINIX_BLOCK_SIZE 1024
#define MINIX_NUM_IPTRS MINIX_BLOCK_SIZE / 4

typedef void (*MinixINodeCallback)(Error error, uint32_t inode, void* udata);

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
} Minix3Superblock;

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
} MinixInode;

typedef struct {
    uint32_t inode;
    uint8_t name[60];
} MinixDirEntry;

typedef struct {
    VfsFilesystem base;
    Minix3Superblock superblock;
    VfsFile* block_device;
    SpinLock lock;
    SpinLock maps_lock;
} MinixFilesystem;

MinixFilesystem* createMinixFilesystem(VfsFile* block_device, VirtPtr data);

size_t offsetForINode(const MinixFilesystem* fs, uint32_t inode);

#endif
