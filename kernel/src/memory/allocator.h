#ifndef _ALLOCATOR_H_
#define _ALLOCATOR_H_

#include <stddef.h>

#define MINIMUM_FREE sizeof(FreeMemory)
#define MIN_BLOCKS_TO_FREE 32

typedef struct FreeMemory_s {
    size_t size; // Number of free bytes after this one
    struct FreeMemory_s* next;
} FreeMemory;

typedef struct Allocator_s {
    size_t block_size;
    struct Allocator_s* backing;
    struct FreeMemory_s* first_free;
} Allocator;

void initAllocator(Allocator* alloc, size_t block_size, Allocator* backing);

void deinitAllocator(Allocator* alloc);

void* allocMemory(Allocator* alloc, size_t size);

void deallocMemory(Allocator* alloc, void* ptr, size_t size);

void* reallocMemory(Allocator* alloc, void* old_ptr, size_t old_size, size_t new_size);

#endif
