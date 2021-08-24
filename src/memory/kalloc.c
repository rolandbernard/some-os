
#include <stdint.h>
#include <string.h>

#include "memory/kalloc.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "util/spinlock.h"

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
uintptr_t next_vaddr = KALLOC_MEM_START;
/* static uintptr_t next_vaddr = KALLOC_MEM_START; */
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
    FreeMemory* mem = (FreeMemory*)next_vaddr;
    for (size_t i = 0; i < size; i++) {
        void* page = allocPage();
        lockSpinLock(&kernel_page_table_lock);
        mapPage(kernel_page_table, next_vaddr, (uintptr_t)page, PAGE_ENTRY_AD_RW, 0);
        unlockSpinLock(&kernel_page_table_lock);
        next_vaddr += PAGE_SIZE;
    }
    mem->size = size * PAGE_SIZE;
    setVirtualMemory(0, kernel_page_table, true);
    insertFreeMemory(mem);
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

void* kalloc(size_t size) {
    if (size == 0) {
        return NULL;
    } else {
        lockSpinLock(&kalloc_lock);
        size_t length = (size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN;
        FreeMemory** memory = findFreeMemoryThatFits(length);
        if (memory == NULL) {
            memory = findFreeMemoryAtEnd();
            if (memory == NULL) {
                addNewMemory(length);
            } else {
                addNewMemory(length - (*memory)->size);
            }
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
    memset(mem, 0, size);
    return mem;
}

static void tryFreeingOldMemory() {
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
        if (length < size_with_header && after != NULL && ((uintptr_t)*after + (*after)->size) == next_vaddr) {
            size_t old_size = (*after)->size;
            addNewMemory(size_with_header - length);
            before = findFreeMemoryBefore(mem);
            after = findFreeMemoryAfter(mem);
            length += (*after)->size - old_size;
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
            memmove(start->bytes, ptr, mem->size - sizeof(AllocatedMemory));
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
            memmove(ret, ptr, mem->size - sizeof(AllocatedMemory));
            dealloc(ptr);
            return ret;
        }
    }
}

