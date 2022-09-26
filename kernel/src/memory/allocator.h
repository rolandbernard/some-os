#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_

#include <stddef.h>

#define MINIMUM_FREE sizeof(FreeMemory)

typedef struct FreeMemory_s {
    size_t size; // Number of free bytes after this one
    struct FreeMemory_s* next;
} FreeMemory;

typedef struct Allocator_s {
    size_t block_size;
    size_t min_backing_free;
    struct Allocator_s* backing;
    struct FreeMemory_s* first_free;
    void* special_range_start;
    void* special_range_end;
} Allocator;

void initAllocator(Allocator* alloc, size_t block_size, Allocator* backing);

void deinitAllocator(Allocator* alloc);

void* allocMemory(Allocator* alloc, size_t size);

void deallocMemory(Allocator* alloc, void* ptr, size_t size);

void* reallocMemory(Allocator* alloc, void* old_ptr, size_t old_size, size_t new_size);

#endif
