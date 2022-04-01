
#include <stdint.h>
#include <string.h>

#include "memory/kalloc.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "task/spinlock.h"
#include "util/util.h"

#define KALLOC_MIN_PAGES_TO_FREE 8
#define KALLOC_MEM_START 0x100000000
#define KALLOC_MEM_ALIGN 8
#define KALLOC_MIN_FREE_MEM sizeof(FreeMemory)

typedef struct FreeMemory_s {
    size_t size; // Number of free bytes
    struct FreeMemory_s* next;
} FreeMemory;

typedef struct AllocatedMemory_s {
    size_t size; // Number of bytes allocated
    uint8_t bytes[];
} AllocatedMemory;

static FreeMemory* first_free = NULL;
static SpinLock kalloc_lock;

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

void* kalloc(size_t size) {
    if (size == 0) {
        return NULL;
    } else {
        lockSpinLock(&kalloc_lock);
        size_t length = (size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN;
        FreeMemory** memory = findFreeMemoryThatFits(length);
        if (memory == NULL) {
            addNewMemory(length);
            memory = findFreeMemoryThatFits(length);
        }
        void* ret = NULL;
        if (memory != NULL) {
            AllocatedMemory* mem = (AllocatedMemory*)*memory;
            if ((*memory)->size < length + KALLOC_MIN_FREE_MEM) {
                *memory = (*memory)->next;
            } else {
                FreeMemory* next = (FreeMemory*)((uintptr_t)*memory + length);
                next->next = (*memory)->next;
                next->size = (*memory)->size - length;
                *memory = next;
                mem->size = length;
            }
            ret = mem->bytes;
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
            (mem_start == page_start && mem_end == page_end) // If this will not create any additional fragmentation
            || page_end >= page_start + KALLOC_MIN_PAGES_TO_FREE * PAGE_SIZE // Or free more than a minimum number of pages
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

void dealloc(void* ptr) {
    if (ptr != NULL) {
        lockSpinLock(&kalloc_lock);
        FreeMemory* mem = (FreeMemory*)(ptr - sizeof(AllocatedMemory));
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
        size_t size_with_header = (size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN;
        lockSpinLock(&kalloc_lock);
        AllocatedMemory* mem = (AllocatedMemory*)(ptr - sizeof(AllocatedMemory));
        FreeMemory** before = findFreeMemoryBefore(mem);
        FreeMemory** after = findFreeMemoryAfter(mem);
        size_t length = mem->size;
        if (before != NULL) {
            length += (*before)->size;
        }
        if (after != NULL) {
            length += (*after)->size;
        }
        if (length >= size_with_header) {
            AllocatedMemory* start = mem;
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
            if (length < size_with_header + KALLOC_MIN_FREE_MEM) {
                start->size = length;
            } else {
                FreeMemory* next = (FreeMemory*)((uintptr_t)start + size_with_header);
                next->next = first_free;
                next->size = length - size_with_header;
                first_free = next;
                start->size = size_with_header;
            }
            tryFreeingOldMemory();
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
        FreeMemory* mem = (FreeMemory*)(ptr - sizeof(AllocatedMemory));
        return mem->size;
    }
}

