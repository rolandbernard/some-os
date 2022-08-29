
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

#define KALLOC_MIN_PAGES_TO_FREE 8
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
#endif

static void insertFreeMemory(FreeMemory* memory) {
    FreeMemory** current = &first_free;
    while (*current != NULL) {
        uintptr_t current_ptr = (uintptr_t)*current;
        uintptr_t memory_ptr = (uintptr_t)memory;
        if (memory_ptr + memory->size == current_ptr) {
            memory->size += (*current)->size;
            *current = (*current)->next;
        } else if (current_ptr + (*current)->size == memory_ptr) {
            (*current)->size += memory->size;
            memory = *current;
            *current = (*current)->next;
        } else {
            current = &(*current)->next;
        }
    }
    memory->next = first_free;
    first_free = memory;
}

static void addNewMemory(size_t size) {
    size = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    FreeMemory* mem = (FreeMemory*)allocPages(size).ptr;
    if (mem != NULL && size > 0) {
        mem->size = size * PAGE_SIZE;
        insertFreeMemory(mem);
    }
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

void* kalloc(size_t size) {
    if (size == 0) {
        return NULL;
    } else {
        lockSpinLock(&kalloc_lock);
        size_t alloc_size = kallocActualSizeFor(size);
        FreeMemory** memory = findFreeMemoryThatFits(alloc_size);
        if (memory == NULL) {
            addNewMemory(alloc_size);
            memory = findFreeMemoryThatFits(alloc_size);
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
        unlockSpinLock(&kalloc_lock);
        return ret;
    }
}

void* zalloc(size_t size) {
    void* mem = kalloc(size);
    if (mem != NULL) {
        memset(mem, 0, size);
    }
    return mem;
}

static void tryFreeingOldMemory() {
    FreeMemory** current = &first_free;
    while ((*current) != NULL) {
        uintptr_t mem_start = (uintptr_t)*current;
        uintptr_t mem_end = mem_start + (*current)->size;
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
                *current = (*current)->next;
            } else if (mem_end == page_end) {
                (*current)->size = page_start - mem_start;
            } else if (mem_start == page_start) {
                FreeMemory* next = (FreeMemory*)page_end;
                next->next = (*current)->next;
                next->size = mem_end - page_end;
                *current = next;
            } else {
                (*current)->size = page_start - mem_start;
                FreeMemory* next = (FreeMemory*)page_end;
                next->next = (*current)->next;
                next->size = mem_end - page_end;
                (*current)->next = next;
            }
            deallocPages(alloc);
        } else {
            current = &(*current)->next;
        }
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
        insertFreeMemory(mem);
        tryFreeingOldMemory();
        unlockSpinLock(&kalloc_lock);
    }
}

static FreeMemory** findFreeMemoryBefore(AllocatedMemory* memory) {
    FreeMemory** current = &first_free;
    while (*current != NULL) {
        uintptr_t current_ptr = (uintptr_t)*current;
        uintptr_t memory_ptr = (uintptr_t)memory;
        if (current_ptr + (*current)->size == memory_ptr) {
            return current;
        } else {
            current = &(*current)->next;
        }
    }
    return NULL;
}

static FreeMemory** findFreeMemoryAfter(AllocatedMemory* memory) {
    FreeMemory** current = &first_free;
    while (*current != NULL) {
        uintptr_t current_ptr = (uintptr_t)*current;
        uintptr_t memory_ptr = (uintptr_t)memory;
        if (memory_ptr + memory->size == current_ptr) {
            return current;
        } else {
            current = &(*current)->next;
        }
    }
    return NULL;
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
        AllocatedMemory* mem = findAllocatedMemoryFor(ptr, false);
        FreeMemory** before = findFreeMemoryBefore(mem);
        FreeMemory** after = findFreeMemoryAfter(mem);
        size_t old_size = mem->size;
        if (before != NULL) {
            old_size += (*before)->size;
        }
        if (after != NULL) {
            old_size += (*after)->size;
        }
        if (old_size >= alloc_size) {
            AllocatedMemory* start = findAllocatedMemoryFor(ptr, true);
            if (before != NULL) {
                start = (AllocatedMemory*)*before;
            }
            if (before != NULL && after == &(*before)->next) {
                *before = (*after)->next;
            } else if (after != NULL && before == &(*after)->next) {
                *after = (*before)->next;
            } else {
                if (before != NULL) {
                    *before = (*before)->next;
                }
                if (after != NULL) {
                    *after = (*after)->next;
                }
            }
            memmove(start->bytes, ptr, umin(mem->size - sizeof(AllocatedMemory), size));
            if (old_size < alloc_size + KALLOC_MIN_FREE_MEM) {
                start->size = old_size;
            } else {
                FreeMemory* next = (FreeMemory*)((uintptr_t)start + alloc_size);
                next->next = first_free;
                next->size = old_size - alloc_size;
                first_free = next;
                start->size = alloc_size;
            }
            tryFreeingOldMemory();
#ifdef DEBUG
            mem->next = allocated;
            allocated = mem;
#endif
            unlockSpinLock(&kalloc_lock);
            return start->bytes;
        } else {
            unlockSpinLock(&kalloc_lock);
            void* ret = kalloc(size);
            if (ret != NULL) {
                memcpy(ret, ptr, umin(mem->size - sizeof(AllocatedMemory), size));
            }
            dealloc(ptr);
            return ret;
        }
    }
}

size_t kallocSize(void* ptr) {
    if (ptr == NULL) {
        return 0;
    } else {
        lockSpinLock(&kalloc_lock);
        AllocatedMemory* mem = findAllocatedMemoryFor(ptr, false);
#ifdef DEBUG
        mem->next = allocated;
        allocated = mem;
#endif
        unlockSpinLock(&kalloc_lock);
        return mem->size;
    }
}

void panicUnlockKalloc() {
    initSpinLock(&kalloc_lock);
}

