
#include <assert.h>

#include "files/vfs/cache.h"

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

static bool continueSearch(VfsNodeCache* table, size_t idx, size_t sb_id, size_t node_id) {
    return table->nodes[idx] == DELETED
           || (isIndexValid(table, idx) && !vfsNodeCompareKey(table->nodes[idx], sb_id, node_id));
}

static size_t findIndexHashTable(VfsNodeCache* table, size_t sb_id, size_t node_id) {
    size_t idx = vfsNodeKeyHash(sb_id, node_id) % table->capacity;
    while (continueSearch(table, idx, sb_id, node_id)) {
        idx = (idx + 1) % table->capacity;
    }
    return idx;
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

VfsNode* vfsCacheGetNodeOrLock(VfsNodeCache* cache, size_t sb_id, size_t node_id) {
    lockTaskLock(&cache->lock);
    size_t idx = findIndexHashTable(cache, sb_id, node_id);
    VfsNode* found = cache->nodes[idx];
    if (found != DELETED && found != EMPTY) {
        unlockTaskLock(&cache->lock);
        return found;
    } else {
        return NULL;
    }
}

void vfsCacheRegisterNodeAndUnlock(VfsNodeCache* cache, VfsNode* node) {
    testForResize(cache);
    size_t idx = insertIndexHashTable(cache, node);
    assert(cache->nodes[idx] == EMPTY || cache->nodes[idx] == DELETED);
    cache->nodes[idx] = node;
    unlockTaskLock(&cache->lock);
}

void vfsCacheCopyNode(VfsNodeCache* cache, VfsNode* node) {
    lockTaskLock(&cache->lock);
    node->ref_count++;
    unlockTaskLock(&cache->lock);
}

void vfsCacheCloseNode(VfsNodeCache* cache, VfsNode* node) {
    lockTaskLock(&cache->lock);
    node->ref_count--;
    if (node->ref_count == 0) {
        size_t idx = findIndexHashTable(cache, node->superblock->id, node->stat.id);
        assert(cache->nodes[idx] == node);
        cache->nodes[idx] = DELETED;
    }
    unlockTaskLock(&cache->lock);
}

