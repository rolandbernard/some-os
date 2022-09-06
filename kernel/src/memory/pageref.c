
#include "memory/pageref.h"
#include "memory/pagealloc.h"
#include "memory/kalloc.h"
#include "util/util.h"

#define MIN_TABLE_CAPACITY 128

static void rebuildPageRefTable(PageRefTable* table, size_t new_size) {
    uintptr_t* new_keys = zalloc(new_size * sizeof(uintptr_t));
    size_t* new_values = zalloc(new_size * sizeof(size_t));
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->values[i] > 1) {
            size_t idx = hashInt64(table->values[i]) % new_size;
            while (new_values[idx] >= 1) {
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

static void testForResize(PageRefTable* table) {
    if (table->capacity < MIN_TABLE_CAPACITY) {
        rebuildPageRefTable(table, MIN_TABLE_CAPACITY);
    } else if (table->capacity > MIN_TABLE_CAPACITY && table->count * 4 < table->capacity) {
        rebuildPageRefTable(table, table->capacity / 2);
    } else if (table->count * 3 > table->capacity * 2) {
        rebuildPageRefTable(table, table->capacity * 3 / 2);
    }
}

bool hasOtherReferences(PageRefTable* table, uintptr_t page) {
    if (page == (uintptr_t)zero_page) {
        // zero page is always referenced
        return true;
    } else if (table->count == 0) {
        return false;
    } else {
        size_t idx = hashInt64(page) % table->capacity;
        while (table->keys[idx] != page && table->values[idx] >= 1) {
            idx = (idx + 1) % table->capacity;
        }
        if (table->keys[idx] == page) {
            return table->values[idx] > 1;
        } else {
            return false;
        }
    }
}

void addReferenceFor(PageRefTable* table, uintptr_t page) {
    // Minimum reference count here must be 2
    testForResize(table);
    size_t idx = hashInt64(page) % table->capacity;
    while (table->keys[idx] != page && table->values[idx] >= 1) {
        idx = (idx + 1) % table->capacity;
    }
    if (table->keys[idx] == page && table->values[idx] > 1) {
        table->values[idx]++;
    } else {
        // The entry does not yet exist
        idx = hashInt64(page) % table->capacity;
        while (table->keys[idx] != page && table->values[idx] > 1) {
            idx = (idx + 1) % table->capacity;
        }
        table->keys[idx] = page;
        table->values[idx] = 2;
        table->count++;
    }
}

void removeReferenceFor(PageRefTable* table, uintptr_t page) {
    if (table->count != 0) {
        size_t idx = hashInt64(page) % table->capacity;
        while (table->keys[idx] != page && table->values[idx] >= 1) {
            idx = (idx + 1) % table->capacity;
        }
        // If the table entry is 1. Don't remove it, because we see it as a deleted value.
        if (table->keys[idx] == page && table->values[idx] > 1) {
            table->values[idx]--;
            if (table->values[idx] == 1) {
                // This entry was just "removed"
                table->count--;
                testForResize(table);
            }
        }
    }
}

