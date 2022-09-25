
#include <assert.h>

#include "memory/allocator.h"

typedef struct FreeMemory_s {
    size_t size; // Number of free bytes after this one
    struct FreeMemory_s* next;
} FreeMemory;

void initAllocator(Allocator* alloc, Allocator* backing) {
    alloc->backing = backing;
    alloc->first_free = NULL;
}

void deinitAllocator(Allocator* alloc) {
    assert(alloc->backing != NULL);
    while (alloc->first_free != NULL) {
    }
}

void* allocMemory(Allocator* alloc, size_t size) {
    return NULL;
}

void deallocMemory(Allocator* alloc, void* ptr, size_t size) {

}

void* reallocMemory(Allocator* alloc, void* old_ptr, size_t old_size, size_t new_size) {
    return NULL;
}

