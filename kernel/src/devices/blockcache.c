
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "memory/allocator.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "task/spinlock.h"
#include "task/types.h"

#include "devices/blockcache.h"

// For now, this is only a read cache. This avoids having to write when reclaiming pages.

typedef struct {
    size_t offset;
    Priority priority;
    uint8_t* bytes;
} CachedBlock;

typedef struct {
    CachedBlock** blocks;
    size_t count;
    size_t capacity;
} CachedBlockTable;

// TODO
// static void resizeTable(CachedBlockTable* table, size_t offset, uint8_t* bytes) {
// }

static void insertIntoTable(CachedBlockTable* table, size_t offset, uint8_t* bytes) {

}

static CachedBlock* getFromTable(CachedBlockTable* table, size_t offset) {
    return NULL;
}

// TODO
// static void removeFromTable(CachedBlockTable* table, size_t offset) {
// }

typedef struct {
    BlockDevice base;
    SpinLock cache_lock;
    BlockDevice* uncached;
    CachedBlockTable table;
    Allocator alloc;
} CachedBlockDevice;

static Error readUncached(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    lockSpinLock(&dev->cache_lock);
    uint8_t* bytes = allocMemory(&dev->alloc, size);
    unlockSpinLock(&dev->cache_lock);
    CHECKED(dev->uncached->functions->read(dev->uncached, virtPtrForKernel(bytes), offset, size), {
        lockSpinLock(&dev->cache_lock);
        deallocMemory(&dev->alloc, bytes, size);
        unlockSpinLock(&dev->cache_lock);
    });
    memcpyBetweenVirtPtr(buff, virtPtrForKernel(bytes), size);
    while (size > 0) {
        lockSpinLock(&dev->cache_lock);
        CachedBlock* cached = getFromTable(&dev->table, offset);
        if (cached == NULL) {
            insertIntoTable(&dev->table, offset, bytes);
        } else {
            deallocMemory(&dev->alloc, bytes, dev->base.block_size);
            if (cached->priority > 0) {
                cached->priority--;
            }
        }
        unlockSpinLock(&dev->cache_lock);
        bytes += dev->base.block_size;
        offset += dev->base.block_size;
        size -= dev->base.block_size;
    }
    return simpleError(SUCCESS);
}

static Error readFunction(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    assert(size % dev->base.block_size == 0 && offset % dev->base.block_size == 0);
    size_t uncached = 0;
    while (size > 0) {
        lockSpinLock(&dev->cache_lock);
        CachedBlock* cached = getFromTable(&dev->table, offset + uncached);
        if (cached != NULL) {
            if (cached->priority > 0) {
                cached->priority--;
            }
            VirtPtr cbuff = buff;
            cbuff.address += uncached;
            memcpyBetweenVirtPtr(cbuff, virtPtrForKernel(cached->bytes), dev->base.block_size);
            unlockSpinLock(&dev->cache_lock);
            if (uncached != 0) {
                CHECKED(readUncached(dev, buff, offset, uncached));
                buff.address += uncached + dev->base.block_size;
                offset += uncached + dev->base.block_size;
                uncached = 0;
            }
        } else {
            unlockSpinLock(&dev->cache_lock);
            uncached += dev->base.block_size;
        }
        size -= dev->base.block_size;
    }
    if (uncached != 0) {
        CHECKED(readUncached(dev, buff, offset, uncached));
    }
    return simpleError(SUCCESS);
}

static Error writeFunction(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    assert(size % dev->base.block_size == 0 && offset % dev->base.block_size == 0);
    CHECKED(dev->uncached->functions->write(dev->uncached, buff, offset, size));
    while (size > 0) {
        lockSpinLock(&dev->cache_lock);
        CachedBlock* cached = getFromTable(&dev->table, offset);
        if (cached == NULL) {
            uint8_t* bytes = allocMemory(&dev->alloc, dev->base.block_size);
            memcpyBetweenVirtPtr(virtPtrForKernel(bytes), buff, dev->base.block_size);
            insertIntoTable(&dev->table, offset, bytes);
        }
        unlockSpinLock(&dev->cache_lock);
        buff.address += dev->base.block_size;
        offset += dev->base.block_size;
        size -= dev->base.block_size;
    }
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
    dev->table.blocks = NULL;
    dev->table.count = 0;
    dev->table.capacity = 0;
    initAllocator(&dev->alloc, dev->base.block_size, &page_allocator);
    return (BlockDevice*)dev;
}

