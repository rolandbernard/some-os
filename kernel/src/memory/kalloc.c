
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "memory/kalloc.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "task/spinlock.h"
#include "task/syscall.h"
#include "util/util.h"

#define KALLOC_MIN_PAGES_TO_FREE 32
#define KALLOC_MEM_ALIGN 8
#define KALLOC_MIN_FREE_MEM sizeof(FreeMemory)

typedef struct FreeMemory_s {
    size_t size; // Number of free bytes
    struct FreeMemory_s* next;
} FreeMemory;

typedef struct AllocatedMemory_s {
    size_t size; // Number of bytes allocated
#ifdef DEBUG
    struct AllocatedMemory_s* next;
#endif
    uint8_t bytes[];
} AllocatedMemory;

// This is a small buffer to be used for early allocation.
static FreeMemory small_buffer[1024] = { { .size = sizeof(small_buffer) } };
static FreeMemory* first_free = small_buffer;
static SpinLock kalloc_lock;

#ifdef DEBUG
static AllocatedMemory* allocated = NULL;

static FreeMemory* findOverlappingFreeMemory(void* start, size_t size) {
    // Small test to prevent double frees
    FreeMemory* current = first_free;
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

static FreeMemory** insertFreeMemory(FreeMemory* memory) {
    assert(findOverlappingFreeMemory(memory, memory->size) == NULL);
    FreeMemory** current = &first_free;
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

static FreeMemory** addNewMemory(size_t size) {
    size = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    FreeMemory* memory = (FreeMemory*)allocPages(size).ptr;
    if (memory != NULL && size > 0) {
        memory->size = size * PAGE_SIZE;
        return insertFreeMemory(memory);
    }
    return NULL;
}

static FreeMemory** findFreeMemoryThatFits(size_t size) {
    FreeMemory** current = &first_free;
    while ((*current) != NULL) {
        if ((*current)->size >= size) {
            return current;
        }
        current = &(*current)->next;
    }
    return NULL;
}

static size_t kallocActualSizeFor(size_t user_size) {
    return umax(
        (user_size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN,
        KALLOC_MIN_FREE_MEM
    );
}

static void* basicKalloc(size_t size) {
    size_t alloc_size = kallocActualSizeFor(size);
    FreeMemory** memory = findFreeMemoryThatFits(alloc_size);
    if (memory == NULL) {
        memory = addNewMemory(alloc_size);
    }
    void* ret = NULL;
    if (memory != NULL) {
        AllocatedMemory* mem = (AllocatedMemory*)*memory;
        if ((*memory)->size < alloc_size + KALLOC_MIN_FREE_MEM) {
            *memory = (*memory)->next;
        } else {
            FreeMemory* next = (FreeMemory*)((uintptr_t)*memory + alloc_size);
            next->next = (*memory)->next;
            next->size = (*memory)->size - alloc_size;
            *memory = next;
            mem->size = alloc_size;
        }
        ret = mem->bytes;
#ifdef DEBUG
        mem->next = allocated;
        allocated = mem;
#endif
    }
    return ret;
}

void* kalloc(size_t size) {
    if (size == 0) {
        return NULL;
    } else {
        lockSpinLock(&kalloc_lock);
        void* res = basicKalloc(size);
        unlockSpinLock(&kalloc_lock);
        return res;
    }
}

void* zalloc(size_t size) {
    void* mem = kalloc(size);
    if (mem != NULL) {
        memset(mem, 0, size);
    }
    return mem;
}

static void tryFreeingOldMemory(FreeMemory** memory) {
    uintptr_t mem_start = (uintptr_t)*memory;
    uintptr_t mem_end = mem_start + (*memory)->size;
    uintptr_t page_start;
    uintptr_t page_end;
    if ((mem_start & -PAGE_SIZE) == mem_start) {
        page_start = mem_start;
    } else {
        page_start = (mem_start + KALLOC_MIN_FREE_MEM + PAGE_SIZE - 1) & -PAGE_SIZE;
    }
    if ((mem_end & -PAGE_SIZE) == mem_end) {
        page_end = mem_end;
    } else {
        page_end = (mem_end - KALLOC_MIN_FREE_MEM) & -PAGE_SIZE;
    }
    if (
        (mem_end <= (uintptr_t)small_buffer || mem_start >= (uintptr_t)small_buffer + sizeof(small_buffer)) // Do not free the small_buffer memory
        &&  ((mem_start == page_start && mem_end == page_end) // If this will not create any additional fragmentation
            || page_end >= page_start + KALLOC_MIN_PAGES_TO_FREE * PAGE_SIZE) // Or free more than a minimum number of pages
    ) {
        PageAllocation alloc = {
            .ptr = (void*)page_start,
            .size = (page_end - page_start) / PAGE_SIZE,
        };
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
        deallocPages(alloc);
    }
}

static AllocatedMemory* findAllocatedMemoryFor(void* ptr, bool remove) {
#ifdef DEBUG
    AllocatedMemory** curr = &allocated;
    while (*curr != NULL) {
        if ((*curr)->bytes == ptr) {
            AllocatedMemory* found = *curr;
            if (remove) {
                *curr = (*curr)->next;
            }
            return found;
        } else {
            curr = &(*curr)->next;
        }
    }
    panic();
#else
    return (AllocatedMemory*)(ptr - sizeof(AllocatedMemory));
#endif
}

void dealloc(void* ptr) {
    if (ptr != NULL) {
        lockSpinLock(&kalloc_lock);
        FreeMemory* mem = (FreeMemory*)findAllocatedMemoryFor(ptr, true);
        tryFreeingOldMemory(insertFreeMemory(mem));
        unlockSpinLock(&kalloc_lock);
    }
}

void* krealloc(void* ptr, size_t size) {
    if (size == 0) {
        dealloc(ptr);
        return NULL;
    } else if (ptr == NULL) {
        return kalloc(size);
    } else {
        size_t alloc_size = kallocActualSizeFor(size);
        lockSpinLock(&kalloc_lock);
        AllocatedMemory* memory = findAllocatedMemoryFor(ptr, true);
        size_t copy_size = umin(memory->size - sizeof(AllocatedMemory), size);
        FreeMemory* free_mem = (FreeMemory*)memory;
        size_t free_size = memory->size;
        assert(findOverlappingFreeMemory(memory, memory->size) == NULL);
        FreeMemory** current = &first_free;
        while ((*current) != NULL && (void*)(*current) < (void*)free_mem) {
            if ((void*)(*current) + (*current)->size == (void*)free_mem) {
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
        if (free_size >= alloc_size) {
            AllocatedMemory* alloc_mem = (AllocatedMemory*)free_mem;
            if (alloc_mem != memory) {
                memmove(alloc_mem->bytes, ptr, copy_size);
            }
            if (free_size < alloc_size + KALLOC_MIN_FREE_MEM) {
                alloc_mem->size = free_size;
            } else {
                free_mem = (FreeMemory*)((void*)free_mem + alloc_size);
                free_mem->next = *current;
                free_mem->size = free_size - alloc_size;
                *current = free_mem;
                tryFreeingOldMemory(current);
                alloc_mem->size = alloc_size;
            }
#ifdef DEBUG
            alloc_mem->next = allocated;
            allocated = alloc_mem;
#endif
            unlockSpinLock(&kalloc_lock);
            return alloc_mem->bytes;
        } else {
            void* new_ptr = basicKalloc(size);
            memmove(new_ptr, ptr, copy_size);
            free_mem->next = *current;
            free_mem->size = free_size;
            *current = free_mem;
            tryFreeingOldMemory(current);
            unlockSpinLock(&kalloc_lock);
            return new_ptr;
        }
    }
}

size_t kallocSize(void* ptr) {
    if (ptr == NULL) {
        return 0;
    } else {
        lockSpinLock(&kalloc_lock);
        AllocatedMemory* mem = findAllocatedMemoryFor(ptr, false);
        unlockSpinLock(&kalloc_lock);
        return mem->size;
    }
}

