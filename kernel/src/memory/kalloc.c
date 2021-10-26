
#include <stdint.h>
#include <string.h>

#include "memory/kalloc.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "util/spinlock.h"
#include "util/util.h"

// Allocating into virtual memory will not allow accessing it in M-mode.
// But allocating physical memory will increase fragmentation.
/* #define KALLOC_VIRT_MEM */

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

#ifdef KALLOC_VIRT_MEM
static uintptr_t next_vaddr = KALLOC_MEM_START;
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
#ifdef KALLOC_VIRT_MEM
    FreeMemory* mem = (FreeMemory*)next_vaddr;
    for (size_t i = 0; i < size; i++) {
        void* page = allocPage();
        if (page == NULL) {
            // Out of memory. Add the memory we were able to allocate.
            size = i;
            break;
        }
        lockSpinLock(&kernel_page_table_lock);
        mapPage(kernel_page_table, next_vaddr, (uintptr_t)page, PAGE_ENTRY_AD_RW, 0);
        unlockSpinLock(&kernel_page_table_lock);
        next_vaddr += PAGE_SIZE;
    }
    setVirtualMemory(0, kernel_page_table, true);
#else
    FreeMemory* mem = (FreeMemory*)allocPages(size).ptr;
#endif
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

#ifdef KALLOC_VIRT_MEM

static FreeMemory** findFreeMemoryAtEnd() {
    FreeMemory** current = &first_free;
    while ((*current) != NULL) {
        if (((uintptr_t)*current + (*current)->size) == next_vaddr) {
            return current;
        }
        current = &(*current)->next;
    }
    return NULL;
}

#endif

void* kalloc(size_t size) {
    if (size == 0) {
        return NULL;
    } else {
        lockSpinLock(&kalloc_lock);
        size_t length = (size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN;
        FreeMemory** memory = findFreeMemoryThatFits(length);
        if (memory == NULL) {
#ifdef KALLOC_VIRT_MEM
            memory = findFreeMemoryAtEnd();
            if (memory == NULL) {
                addNewMemory(length);
            } else {
                addNewMemory(length - (*memory)->size);
            }
#else
            addNewMemory(length);
#endif
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
#ifdef KALLOC_VIRT_MEM
    FreeMemory** mem = findFreeMemoryAtEnd();
    if (mem != NULL) {
        uintptr_t mem_start = (uintptr_t)*mem;
        uintptr_t mem_end = mem_start + (*mem)->size;
        uintptr_t free_start;
        if ((mem_start & -PAGE_SIZE) == mem_start) {
            free_start = mem_start;
            *mem = (*mem)->next;
        } else {
            free_start = (mem_start + KALLOC_MIN_FREE_MEM + PAGE_SIZE - 1) & -PAGE_SIZE;
            (*mem)->size = free_start - mem_start;
        }
        if (free_start < mem_end) {
            size_t size = (mem_end - free_start) / PAGE_SIZE;
            for (size_t i = 0; i < size; i++) {
                next_vaddr -= PAGE_SIZE;
                void* page = (void*)virtToPhys(kernel_page_table, next_vaddr);
                deallocPage(page);
                lockSpinLock(&kernel_page_table_lock);
                unmapPage(kernel_page_table, next_vaddr);
                unlockSpinLock(&kernel_page_table_lock);
            }
        }
        setVirtualMemory(0, kernel_page_table, true);
    }
#else
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
#endif
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
#ifdef KALLOC_VIRT_MEM
        if (length < size_with_header && after != NULL && ((uintptr_t)*after + (*after)->size) == next_vaddr) {
            size_t old_size = (*after)->size;
            addNewMemory(size_with_header - length);
            before = findFreeMemoryBefore(mem);
            after = findFreeMemoryAfter(mem);
            length += (*after)->size - old_size;
        }
#endif
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

