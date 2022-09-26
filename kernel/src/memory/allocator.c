
#include <assert.h>
#include <string.h>

#include "util/util.h"

#include "memory/allocator.h"

typedef struct FreeMemory_s {
    size_t size; // Number of free bytes after this one
    struct FreeMemory_s* next;
} FreeMemory;

#define MINIMUM_FREE sizeof(FreeMemory)
#define MIN_BLOCKS_TO_FREE 32

void initAllocator(Allocator* alloc, size_t block_size, Allocator* backing) {
    assert(backing->block_size % block_size == 0);
    alloc->block_size = block_size;
    alloc->backing = backing;
    alloc->first_free = NULL;
}

void deinitAllocator(Allocator* alloc) {
    assert(alloc->backing != NULL);
    while (alloc->first_free != NULL) {
        FreeMemory* free = alloc->first_free;
        alloc->first_free = free->next;
        deallocMemory(alloc->backing, free, free->size);
    }
}

#ifdef DEBUG
static FreeMemory* findOverlappingFreeMemory(Allocator* alloc, void* start, size_t size) {
    // Small test to prevent double frees
    FreeMemory* current = alloc->first_free;
    while (current != NULL) {
        assert(
            start + size <= (void*)current
            || start >= (void*)current + current->size
        );
        current = current->next;
    }
    return NULL;
}
#endif

static FreeMemory** insertFreeMemory(Allocator* alloc, FreeMemory* memory) {
    assert(findOverlappingFreeMemory(alloc, memory, memory->size) == NULL);
    FreeMemory** current = &alloc->first_free;
    while ((*current) != NULL && (void*)(*current) < (void*)memory) {
        if ((void*)(*current) + (*current)->size == (void*)memory) {
            (*current)->size += memory->size;
            memory = *current;
            *current = (*current)->next;
        } else {
            current = &(*current)->next;
        }
    }
    if ((*current) != NULL && (void*)memory + memory->size == (*current)) {
        memory->next = (*current)->next;
        memory->size += (*current)->size;
    } else {
        memory->next = (*current);
    }
    *current = memory;
    return current;
}

static FreeMemory** findFreeMemoryThatFits(Allocator* alloc, size_t size) {
    FreeMemory** current = &alloc->first_free;
    while ((*current) != NULL) {
        if ((*current)->size >= size) {
            return current;
        }
        current = &(*current)->next;
    }
    return NULL;
}

static FreeMemory** addNewMemory(Allocator* alloc, size_t size) {
    assert(size > 0);
    size_t backing_size = (size + alloc->backing->block_size - 1);
    backing_size -= backing_size % alloc->backing->block_size;
    FreeMemory* new_free = allocMemory(alloc->backing, backing_size);
    if (new_free != NULL) {
        new_free->size = backing_size;
        return insertFreeMemory(alloc, new_free);
    }
    return NULL;
}

static void* basicAllocMemory(Allocator* alloc, size_t size) {
    assert(size % alloc->block_size == 0);
    FreeMemory** memory = findFreeMemoryThatFits(alloc, size);
    if (memory == NULL && alloc->backing != NULL) {
        memory = addNewMemory(alloc, size);
    }
    void* ret = NULL;
    if (memory != NULL) {
        size_t min_size = (size + MINIMUM_FREE + alloc->block_size - 1);
        min_size -= min_size % alloc->block_size;
        if ((*memory)->size >= min_size) {
            FreeMemory* free = *memory;
            FreeMemory* moved = (FreeMemory*)((uintptr_t)free + size);
            moved->size = free->size - size;
            moved->next = free->next;
            *memory = moved;
            return free;
        } else if ((*memory)->size >= size) {
            FreeMemory* free = *memory;
            *memory = free->next;
            return free;
        }
    }
    return ret;
}

void* allocMemory(Allocator* alloc, size_t size) {
    assert(size % alloc->block_size == 0);
    if (size == 0) {
        return 0;
    } else {
        return basicAllocMemory(alloc, size);
    }
}

static void tryFreeingOldMemory(Allocator* alloc, FreeMemory** memory) {
    uintptr_t mem_start = (uintptr_t)*memory;
    uintptr_t mem_end = mem_start + (*memory)->size;
    uintptr_t page_start = mem_start;
    page_start -= mem_start % alloc->backing->block_size;
    uintptr_t page_end = mem_end;
    mem_end -= mem_end % alloc->backing->block_size;
    if (page_start != mem_start) {
        page_start = mem_start + MINIMUM_FREE + alloc->backing->block_size - 1;
        page_start -= mem_start % alloc->backing->block_size;
    }
    if (page_end != mem_end) {
        page_end = mem_end - MINIMUM_FREE;
        page_end -= mem_end % alloc->backing->block_size;
    }
    if (
        ((mem_start == page_start && mem_end == page_end) // If this will not create any additional fragmentation
        || page_end >= page_start + MIN_BLOCKS_TO_FREE * alloc->backing->block_size) // Or free more than a minimum number of pages
    ) {
        void* ptr = (void*)page_start;
        size_t size = page_end - page_start;
        if (page_start == mem_start && page_end == mem_end) {
            *memory = (*memory)->next;
        } else if (mem_end == page_end) {
            (*memory)->size = page_start - mem_start;
        } else if (mem_start == page_start) {
            FreeMemory* next = (FreeMemory*)page_end;
            next->next = (*memory)->next;
            next->size = mem_end - page_end;
            *memory = next;
        } else {
            (*memory)->size = page_start - mem_start;
            FreeMemory* next = (FreeMemory*)page_end;
            next->next = (*memory)->next;
            next->size = mem_end - page_end;
            (*memory)->next = next;
        }
        deallocMemory(alloc->backing, ptr, size);
    }
}

void deallocMemory(Allocator* alloc, void* ptr, size_t size) {
    assert(size % alloc->block_size == 0);
    if (size != 0) {
        FreeMemory* mem = ptr;
        mem->size = size;
        tryFreeingOldMemory(alloc, insertFreeMemory(alloc, mem));
    }
}

void* reallocMemory(Allocator* alloc, void* old_ptr, size_t old_size, size_t new_size) {
    assert(old_size % alloc->block_size == 0 && new_size % alloc->block_size == 0);
    if (old_size == 0) {
        return allocMemory(alloc, new_size);
    } else if (new_size) {
        deallocMemory(alloc, old_ptr, old_size);
        return NULL;
    } else {
        size_t copy_size = umin(new_size, old_size);
        FreeMemory* free_mem = (FreeMemory*)old_ptr;
        size_t free_size = old_size;
        assert(findOverlappingFreeMemory(alloc, old_ptr, old_size) == NULL);
        FreeMemory** current = &alloc->first_free;
        while ((*current) != NULL && (void*)(*current) < (void*)free_mem) {
            if ((void*)(*current) + (*current)->size == old_ptr) {
                free_size += (*current)->size;
                free_mem = *current;
                *current = (*current)->next;
            } else {
                current = &(*current)->next;
            }
        }
        if ((*current) != NULL && (void*)free_mem + free_size == (*current)) {
            free_size += (*current)->size;
            *current = (*current)->next;
        }
        if (free_size >= new_size) {
            if (free_mem != old_ptr) {
                memmove(free_mem, old_ptr, copy_size);
            }
            size_t min_size = (new_size + MINIMUM_FREE + alloc->block_size - 1);
            min_size -= min_size % alloc->block_size;
            if (free_size >= min_size) {
                free_mem = (FreeMemory*)((void*)free_mem + new_size);
                free_mem->next = *current;
                free_mem->size = free_size - new_size;
                *current = free_mem;
                tryFreeingOldMemory(alloc, current);
            }
            return free_mem;
        } else {
            void* new_ptr = basicAllocMemory(alloc, new_size);
            memmove(new_ptr, old_ptr, copy_size);
            free_mem->next = *current;
            free_mem->size = free_size;
            *current = free_mem;
            tryFreeingOldMemory(alloc, current);
            return new_ptr;
        }
    }
    return NULL;
}

