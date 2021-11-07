
#include <string.h>

#include "util/stringmap.h"

#include "memory/kalloc.h"
#include "util/util.h"
#include "files/path.h"

#define EMPTY (void*)0
#define DELETED (void*)1

#define MIN_TABLE_CAPACITY 128

static void rebuildStringMap(StringMap* table, size_t new_size) {
    char** new_keys = zalloc(new_size * sizeof(char*));
    void** new_values = zalloc(new_size * sizeof(void*));
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->values[i] != EMPTY && table->values[i] != DELETED) {
            size_t idx = hashString(table->values[i]) % new_size;
            while (new_values[idx] != EMPTY) {
                idx = (idx + 1) % new_size;
            }
            new_keys[idx] = table->keys[i];
            new_values[idx] = table->values[i];
        }
    }
    dealloc(table->keys);
    dealloc(table->values);
    table->keys = new_keys;
    table->values = new_values;
    table->capacity = new_size;
}

static void testForResize(StringMap* map) {
    if (map->capacity < MIN_TABLE_CAPACITY) {
        rebuildStringMap(map, MIN_TABLE_CAPACITY);
    } else if (map->capacity > MIN_TABLE_CAPACITY && map->count * 4 < map->capacity) {
        rebuildStringMap(map, map->capacity / 2);
    } else if (map->count * 3 > map->capacity * 2) {
        rebuildStringMap(map, map->capacity + map->capacity / 2);
    }
}

static bool areKeysEqual(const char* in_table, const char* key) {
    if (in_table == EMPTY || in_table == DELETED) {
        return false;
    } else {
        return strcmp(in_table, key) == 0;
    }
}

void putToStringMap(StringMap* map, const char* key, void* value) {
    testForResize(map);
    size_t idx = hashString(key) % map->capacity;
    while (map->keys[idx] != EMPTY && !areKeysEqual(map->keys[idx], key)) {
        idx = (idx + 1) % map->capacity;
    }
    if (areKeysEqual(map->keys[idx], key)) {
        map->values[idx] = value;
    } else {
        // The entry does not yet exist
        size_t idx = hashString(key) % map->capacity;
        while (map->keys[idx] != EMPTY && map->keys[idx] != DELETED && !areKeysEqual(map->keys[idx], key)) {
            idx = (idx + 1) % map->capacity;
        }
        map->keys[idx] = stringClone(key);
        map->values[idx] = value;
        map->count++;
    }
}

void* getFromStringMap(StringMap* map, const char* key) {
    if (map->count != 0) {
        size_t idx = hashString(key) % map->capacity;
        while (map->keys[idx] != EMPTY && !areKeysEqual(map->keys[idx], key)) {
            idx = (idx + 1) % map->capacity;
        }
        if (areKeysEqual(map->keys[idx], key)) {
            return map->values[idx];
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

const char* getKeyFromStringMap(StringMap* map, const char* key) {
    if (map->count != 0) {
        size_t idx = hashString(key) % map->capacity;
        while (map->keys[idx] != EMPTY && !areKeysEqual(map->keys[idx], key)) {
            idx = (idx + 1) % map->capacity;
        }
        if (areKeysEqual(map->keys[idx], key)) {
            return map->keys[idx];
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

void deleteFromStringMap(StringMap* map, const char* key) {
    if (map->count != 0) {
        size_t idx = hashString(key) % map->capacity;
        while (map->keys[idx] != EMPTY && !areKeysEqual(map->keys[idx], key)) {
            idx = (idx + 1) % map->capacity;
        }
        if (areKeysEqual(map->keys[idx], key)) {
            dealloc(map->keys[idx]);
            map->keys[idx] = DELETED;
            map->count--;
            testForResize(map);
        }
    }
}

