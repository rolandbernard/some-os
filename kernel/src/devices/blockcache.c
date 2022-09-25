
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "memory/kalloc.h"
#include "task/spinlock.h"

#include "devices/blockcache.h"

// For now, this is only a read cache. This avoids having to write when reclaiming pages.

typedef struct {
    size_t offset;
    uint8_t* bytes;
} CachedBlock;

typedef struct {
    BlockDevice base;
    BlockDevice* uncached;
    SpinLock cache_lock;
    CachedBlock** blocks;
    size_t count;
    size_t capacity;
} CachedBlockDevice;

static Error readFunction(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    return simpleError(ENOSYS);
}

static Error writeFunction(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    return simpleError(ENOSYS);
}

static const BlockDeviceFunctions funcs = {
    .read = (BlockDeviceReadFunction)readFunction,
    .write = (BlockDeviceWriteFunction)writeFunction,
};

BlockDevice* wrapBlockDeviceWithCache(BlockDevice* uncached) {
    CachedBlockDevice* dev = kalloc(sizeof(CachedBlockDevice));
    memcpy(dev, uncached, sizeof(BlockDevice));
    dev->base.functions = &funcs;
    dev->uncached = uncached;
    initSpinLock(&dev->cache_lock);
    dev->blocks = NULL;
    dev->count = 0;
    dev->capacity = 0;
    return (BlockDevice*)dev;
}

