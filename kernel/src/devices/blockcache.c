
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "memory/allocator.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "memory/reclaim.h"
#include "task/spinlock.h"
#include "task/types.h"
#include "util/util.h"

#include "devices/blockcache.h"

// For now, this is only a read cache. This avoids having to write when reclaiming pages.

#define MIN_TABLE_CAPACITY 32

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

static CachedBlock* lookupCachedBlock(CachedBlock** blocks, size_t size, size_t offset) {
    size_t pl = 0;
    size_t idx = hashInt64(offset) % size;
    while (blocks[idx] != NULL && blocks[idx]->offset != offset) {
        size_t oth_pl = (size + idx - (hashInt64(blocks[idx]->offset) % size)) % size;
        if (oth_pl < pl) {
            return NULL;
        }
        idx = (idx + 1) % size;
        pl++;
    }
    return blocks[idx];
}

static void insertCachedBlock(CachedBlock** blocks, size_t size, CachedBlock* block) {
    size_t pl = 0;
    size_t idx = hashInt64(block->offset) % size;
    while (blocks[idx] != NULL && blocks[idx]->offset != block->offset) {
        size_t oth_pl = (size + idx - (hashInt64(blocks[idx]->offset) % size)) % size;
        if (oth_pl < pl) {
            CachedBlock* oth = blocks[idx];
            blocks[idx] = block;
            block = oth;
            pl = oth_pl;
        }
        idx = (idx + 1) % size;
        pl++;
    }
    blocks[idx] = block;
}

static CachedBlock* removeCachedBlock(CachedBlock** blocks, size_t size, size_t offset) {
    size_t pl = 0;
    size_t idx = hashInt64(offset) % size;
    while (blocks[idx] != NULL && blocks[idx]->offset != offset) {
        size_t oth_pl = (size + idx - (hashInt64(blocks[idx]->offset) % size)) % size;
        if (oth_pl < pl) {
            return NULL;
        }
        idx = (idx + 1) % size;
        pl++;
    }
    CachedBlock* removed = blocks[idx];
    if (removed != NULL) {
        while (blocks[(idx + 1) % size] != NULL) {
            size_t nxt_idx = (idx + 1) % size;
            size_t oth_pl = (size + nxt_idx - (hashInt64(blocks[nxt_idx]->offset) % size)) % size;
            if (oth_pl == 0) {
                break;
            }
            blocks[idx] = blocks[nxt_idx];
            idx = nxt_idx;
        }
        blocks[idx] = NULL;
    }
    return removed;
}

static void rebuildTable(CachedBlockTable* table, size_t new_size) {
    CachedBlock** new_blocks = zalloc(new_size * sizeof(CachedBlock*));
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->blocks[i] != NULL) {
            insertCachedBlock(new_blocks, new_size, table->blocks[i]);
        }
    }
    dealloc(table->blocks);
    table->blocks = new_blocks;
    table->capacity = new_size;
}

static void testForResize(CachedBlockTable* table) {
    size_t new_size = table->capacity;
    while (new_size > MIN_TABLE_CAPACITY && table->count * 4 < new_size) {
        new_size /= 2;
    }
    if (new_size < MIN_TABLE_CAPACITY) {
        new_size = MIN_TABLE_CAPACITY;
    }
    while (table->count * 3 > new_size * 2) {
        new_size = new_size * 3 / 2;
    }
    if (new_size != table->capacity) {
        rebuildTable(table, new_size);
    }
}

static void insertIntoTable(CachedBlockTable* table, size_t offset, uint8_t* bytes) {
    testForResize(table);
    CachedBlock* block = kalloc(sizeof(CachedBlock));
    block->priority = DEFAULT_PRIORITY;
    block->offset = offset;
    block->bytes = bytes;
    insertCachedBlock(table->blocks, table->capacity, block);
    table->count++;
}

static CachedBlock* getFromTable(CachedBlockTable* table, size_t offset) {
    if (table->count == 0) {
        return NULL;
    } else {
        return lookupCachedBlock(table->blocks, table->capacity, offset);
    }
}

typedef struct {
    BlockDevice base;
    SpinLock cache_lock;
    BlockDevice* uncached;
    CachedBlockTable table;
    Allocator alloc;
} CachedBlockDevice;

static Error readUncached(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    uint8_t* bytes = allocMemory(&dev->alloc, size);
    unlockSpinLock(&dev->cache_lock);
    CHECKED(dev->uncached->functions->read(dev->uncached, virtPtrForKernel(bytes), offset, size), {
        lockSpinLock(&dev->cache_lock);
        deallocMemory(&dev->alloc, bytes, size);
    });
    memcpyBetweenVirtPtr(buff, virtPtrForKernel(bytes), size);
    lockSpinLock(&dev->cache_lock);
    while (size > 0) {
        CachedBlock* cached = getFromTable(&dev->table, offset);
        if (cached == NULL) {
            insertIntoTable(&dev->table, offset, bytes);
        } else {
            deallocMemory(&dev->alloc, bytes, dev->base.block_size);
            if (cached->priority > 0) {
                cached->priority--;
            }
        }
        bytes += dev->base.block_size;
        offset += dev->base.block_size;
        size -= dev->base.block_size;
    }
    return simpleError(SUCCESS);
}

static Error readFunction(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    assert(size % dev->base.block_size == 0 && offset % dev->base.block_size == 0);
    size_t uncached = 0;
    lockSpinLock(&dev->cache_lock);
    while (size > 0) {
        CachedBlock* cached = getFromTable(&dev->table, offset + uncached);
        if (cached != NULL) {
            if (cached->priority > 0) {
                cached->priority--;
            }
            VirtPtr cbuff = buff;
            cbuff.address += uncached;
            memcpyBetweenVirtPtr(cbuff, virtPtrForKernel(cached->bytes), dev->base.block_size);
            if (uncached != 0) {
                CHECKED(readUncached(dev, buff, offset, uncached), unlockSpinLock(&dev->cache_lock));
            }
            buff.address += uncached + dev->base.block_size;
            offset += uncached + dev->base.block_size;
            uncached = 0;
        } else {
            uncached += dev->base.block_size;
        }
        size -= dev->base.block_size;
    }
    if (uncached != 0) {
        CHECKED(readUncached(dev, buff, offset, uncached), unlockSpinLock(&dev->cache_lock));
    }
    unlockSpinLock(&dev->cache_lock);
    return simpleError(SUCCESS);
}

static Error writeFunction(CachedBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    assert(size % dev->base.block_size == 0 && offset % dev->base.block_size == 0);
    CHECKED(dev->uncached->functions->write(dev->uncached, buff, offset, size));
    lockSpinLock(&dev->cache_lock);
    while (size > 0) {
        CachedBlock* cached = getFromTable(&dev->table, offset);
        if (cached == NULL) {
            uint8_t* bytes = allocMemory(&dev->alloc, dev->base.block_size);
            memcpyBetweenVirtPtr(virtPtrForKernel(bytes), buff, dev->base.block_size);
            insertIntoTable(&dev->table, offset, bytes);
        } else {
            memcpyBetweenVirtPtr(virtPtrForKernel(cached->bytes), buff, dev->base.block_size);
        }
        buff.address += dev->base.block_size;
        offset += dev->base.block_size;
        size -= dev->base.block_size;
    }
    unlockSpinLock(&dev->cache_lock);
    return simpleError(SUCCESS);
}

static const BlockDeviceFunctions funcs = {
    .read = (BlockDeviceReadFunction)readFunction,
    .write = (BlockDeviceWriteFunction)writeFunction,
};

static bool reclaimFunction(Priority priority, CachedBlockDevice* dev) {
    size_t freed = 0;
    lockSpinLock(&dev->cache_lock);
    for (size_t i = 0; i < dev->table.capacity;) {
        CachedBlock* block = dev->table.blocks[i];
        if (block != NULL) {
            if (block->priority >= priority) {
                removeCachedBlock(dev->table.blocks, dev->table.capacity, block->offset);
                dev->table.count--;
                deallocMemory(&dev->alloc, block->bytes, dev->base.block_size);
                dealloc(block);
                freed++;
            } else {
                block->priority++;
                i++;
            }
        } else {
            i++;
        }
    }
    testForResize(&dev->table);
    unlockSpinLock(&dev->cache_lock);
    return freed != 0;
}

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
    registerReclaimable(LOWEST_PRIORITY, (ReclaimFunction)reclaimFunction, dev);
    return (BlockDevice*)dev;
}

