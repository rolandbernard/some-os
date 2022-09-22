
#include <assert.h>

#include "files/vfs/cache.h"

#include "files/vfs/node.h"
#include "memory/kalloc.h"
#include "util/util.h"

#define MIN_TABLE_CAPACITY 32

#define DELETED (VfsNode*)1
#define EMPTY NULL

static bool vfsNodeCompareKey(VfsNode* node, size_t sb_id, size_t node_id) {
    return node->stat.id == node_id && node->superblock->id == sb_id;
}

static size_t vfsNodeKeyHash(size_t sb_id, size_t node_id) {
    return hashCombine(hashInt64(sb_id), hashInt64(node_id));
}

static bool isIndexValid(VfsNodeCache* table, size_t idx) {
    return table->nodes[idx] != EMPTY && table->nodes[idx] != DELETED;
}

static bool continueInsertSearch(VfsNodeCache* table, size_t idx, VfsNode* node) {
    return isIndexValid(table, idx)
           && !vfsNodeCompareKey(table->nodes[idx], node->superblock->id, node->stat.id);
}

static size_t insertIndexHashTable(VfsNodeCache* table, VfsNode* node) {
    size_t idx = vfsNodeKeyHash(node->superblock->id, node->stat.id) % table->capacity;
    while (continueInsertSearch(table, idx, node)) {
        idx = (idx + 1) % table->capacity;
    }
    return idx;
}

static void rebuildHashTable(VfsNodeCache* table, size_t new_size) {
    VfsNodeCache new;
    new.capacity = new_size;
    new.nodes = zalloc(sizeof(VfsNode*) * new_size);
    for (size_t i = 0; i < table->capacity; i++) {
        if (isIndexValid(table, i)) {
            size_t idx = insertIndexHashTable(&new, table->nodes[i]);
            new.nodes[idx] = table->nodes[i];
        }
    }
    dealloc(table->nodes);
    table->nodes = new.nodes;
    table->capacity = new_size;
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

static bool continueSearch(VfsNodeCache* table, size_t idx, size_t sb_id, size_t node_id) {
    return table->nodes[idx] == DELETED
           || (isIndexValid(table, idx) && !vfsNodeCompareKey(table->nodes[idx], sb_id, node_id));
}

static size_t findIndexHashTable(VfsNodeCache* table, size_t sb_id, size_t node_id) {
    if (table->count == 0) {
        return SIZE_MAX;
    }
    size_t start = vfsNodeKeyHash(sb_id, node_id) % table->capacity;
    size_t first_free = SIZE_MAX;
    size_t idx = start;
    while (continueSearch(table, idx, sb_id, node_id)) {
        if (table->nodes[idx] == DELETED) {
            first_free = idx;
        }
        idx = (idx + 1) % table->capacity;
        if (idx == start) {
            return SIZE_MAX;
        }
    }
    VfsNode* found = table->nodes[idx];
    if (found == DELETED || found == EMPTY) {
        return SIZE_MAX;
    } else if (first_free != SIZE_MAX) {
        table->nodes[first_free] = found;
        table->nodes[idx] = DELETED;
        return first_free;
    } else {
        return idx;
    }
}

VfsNode* vfsCacheGetNodeOrLock(VfsNodeCache* cache, size_t sb_id, size_t node_id) {
    lockTaskLock(&cache->lock);
    size_t idx = findIndexHashTable(cache, sb_id, node_id);
    if (idx != SIZE_MAX) {
        VfsNode* found = cache->nodes[idx];
        vfsNodeCopy(found);
        unlockTaskLock(&cache->lock);
        return found;
    } else {
        return NULL;
    }
}

static void vfsCacheInsertNewNode(VfsNodeCache* cache, VfsNode* node) {
    testForResize(cache);
    size_t idx = insertIndexHashTable(cache, node);
    assert(cache->nodes[idx] == EMPTY || cache->nodes[idx] == DELETED);
    cache->nodes[idx] = node;
    cache->count++;
}

void vfsCacheUnlock(VfsNodeCache* cache) {
    unlockTaskLock(&cache->lock);
}

size_t vfsCacheCopyNode(VfsNodeCache* cache, VfsNode* node) {
    lockTaskLock(&cache->lock);
    if (node->ref_count == 0) {
        // This is a new node, add it to the cache.
        vfsCacheInsertNewNode(cache, node);
    }
    node->ref_count++;
    size_t refs = node->ref_count;
    unlockTaskLock(&cache->lock);
    return refs;
}

size_t vfsCacheCloseNode(VfsNodeCache* cache, VfsNode* node) {
    lockTaskLock(&cache->lock);
    assert(node->ref_count > 0);
    node->ref_count--;
    if (node->ref_count == 0) {
        size_t idx = findIndexHashTable(cache, node->superblock->id, node->stat.id);
        assert(idx != SIZE_MAX && cache->nodes[idx] == node);
        cache->nodes[idx] = DELETED;
        cache->count--;
    }
    size_t refs = node->ref_count;
    unlockTaskLock(&cache->lock);
    return refs;
}

void vfsCacheInit(VfsNodeCache* cache) {
    cache->count = 0;
    cache->capacity = 0;
    cache->nodes = NULL;
    initTaskLock(&cache->lock);
}

void vfsCacheDeinit(VfsNodeCache* cache) {
    dealloc(cache->nodes);
}

