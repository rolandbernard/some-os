
#include "files/vfs/cache.h"

#include "util/util.h"

#define MIN_TABLE_CAPACITY 32

#define DELETED (VfsNode*)1
#define EMPTY NULL

static bool vfsNodeCompareKey(const VfsNode* a, const VfsNode* b) {
    return a->stat.id == b->stat.id && a->superblock->id == b->superblock->id;
}

static size_t vfsNodeKeyHash(const VfsNode* node) {
    return hashCombine(hashInt64(node->superblock->id), hashInt64(node->stat.id));
}

static bool isIndexValid(VfsNodeCache* table, size_t idx) {
    return table->nodes[idx] != EMPTY && table->nodes[idx] != DELETED;
}

static bool continueSearch(VfsNodeCache* table, size_t idx, VfsNode* node) {
    return table->nodes[idx] == DELETED
           || (isIndexValid(table, idx) && !vfsNodeCompareKey(table->nodes[idx], node));
}

static size_t findIndexHashTable(VfsNodeCache* table, VfsNode* node) {
    size_t idx = vfsNodeKeyHash(node) % table->capacity;
    while (continueSearch(table, idx, node)) {
        idx = (idx + 1) % table->capacity;
    }
    return idx;
}

static bool continueInsertSearch(VfsNodeCache* table, size_t idx, VfsNode* node) {
    return isIndexValid(table, idx) && !vfsNodeCompareKey(table->nodes[idx], node);
}

static size_t insertIndexHashTable(VfsNodeCache* table, VfsNode* node) {
    size_t idx = vfsNodeKeyHash(node) % table->capacity;
    while (continueInsertSearch(table, idx, node)) {
        idx = (idx + 1) % table->capacity;
    }
    return idx;
}

static void rebuildHashTable(VfsNodeCache* table, size_t new_size) {
    // TODO
}

static void testForResize(VfsNodeCache* table) {
    if (table->capacity < MIN_TABLE_CAPACITY) {
        rebuildHashTable(table, MIN_TABLE_CAPACITY);
    } else if (table->capacity > MIN_TABLE_CAPACITY && table->count * 4 < table->capacity) {
        rebuildHashTable(table, table->capacity / 2);
    } else if (table->count * 3 > table->capacity * 2) {
        rebuildHashTable(table, table->capacity + table->capacity / 2);
    }
}

VfsNode* vfsCacheGetNodeOrLock(VfsNodeCache* cache, size_t sb_id, size_t node_id) {
    lockTaskLock(&cache->lock);
    VfsNode* found;
    // TODO
    if (found != DELETED && found != EMPTY) {
        unlockTaskLock(&cache->lock);
        return found;
    } else {
        return NULL;
    }
}

void vfsCacheRegisterNodeAndUnlock(VfsNodeCache* cache, VfsNode* node) {
    // TODO
    unlockTaskLock(&cache->lock);
}

void vfsCacheCopyNode(VfsNodeCache* cache, VfsNode* node) {
    lockTaskLock(&cache->lock);
    node->ref_count++;
    unlockTaskLock(&cache->lock);
}

void vfsCacheCloseNode(VfsNodeCache* cache, VfsNode* node) {
    lockTaskLock(&cache->lock);
    // TODO
    unlockTaskLock(&cache->lock);
}

