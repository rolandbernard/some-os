
#include <stdint.h>
#include <string.h>

#include "memory/kalloc.h"

#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "util/spinlock.h"

#define KALLOC_MEM_START 0x90000000
#define KALLOC_MIN_FREE_MEM 16
#define KALLOC_MEM_ALIGN 8

typedef struct FreeMemory_s {
    size_t size; // Number of free bytes
    struct FreeMemory_s* next;
} FreeMemory;

typedef struct AllocatedMemory_s {
    size_t size; // Number of bytes allocated
    uint8_t bytes[];
} AllocatedMemory;

static FreeMemory* first_free = NULL;
static uintptr_t next_vaddr = KALLOC_MEM_START;
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
            *current = (*current)->next;
            memory = *current;
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
        if ((*current)->size >= size + sizeof(AllocatedMemory)) {
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
        FreeMemory** memory = findFreeMemoryThatFits(size);
        if (memory == NULL) {
            addNewMemory(size);
            memory = findFreeMemoryThatFits(size);
        }
        void* ret = NULL;
        if (memory != NULL) {
            AllocatedMemory* mem = (AllocatedMemory*)*memory;
            size_t length = (size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN;
            if ((*memory)->size < length + KALLOC_MIN_FREE_MEM) {
                *memory = (*memory)->next;
            } else {
                mem->size = length;
                FreeMemory* next = (FreeMemory*)(((uintptr_t)*memory) + length);
                next->size = (*memory)->size - length;
                next->next = (*memory)->next;
                *memory = next;
            }
            ret = &mem->bytes;
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

void dealloc(void* ptr) {
    if (ptr != NULL) {
        lockSpinLock(&kalloc_lock);
        FreeMemory* mem = (FreeMemory*)(ptr - sizeof(AllocatedMemory));
        insertFreeMemory(mem);
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
        lockSpinLock(&kalloc_lock);
        size_t size_with_header = (size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN;
        AllocatedMemory* mem = (AllocatedMemory*)(ptr - sizeof(AllocatedMemory));
        FreeMemory** before = findFreeMemoryBefore(mem);
        FreeMemory** after = findFreeMemoryAfter(mem);
        size_t length = mem->size;
        if (length < size_with_header) {
            if (before != NULL) {
                length += (*before)->size;
            }
        }
        if (length < size_with_header) {
            if (after != NULL) {
                length += (*after)->size;
            }
        }
        if (length < size_with_header) {
            AllocatedMemory* start = mem;
            length = mem->size;
            if (length < size_with_header) {
                if (before != NULL) {
                    if (length + (*before)->size < size_with_header + KALLOC_MIN_FREE_MEM) {
                        start = (AllocatedMemory*)*before;
                        length += (*before)->size;
                        *before = (*before)->next;
                    } else {
                        start = (AllocatedMemory*)(*before + (size_with_header - length));
                        length = size_with_header;
                        (*before)->size -= (size_with_header - length);
                    }
                }
            }
            if (length < size_with_header) {
                if (after != NULL) {
                    if (length + (*after)->size < size_with_header + KALLOC_MIN_FREE_MEM) {
                        length += (*after)->size;
                        *after = (*after)->next;
                    } else {
                        length = size_with_header;
                        FreeMemory* next = (FreeMemory*)(((uintptr_t)*after) + (size_with_header - length));
                        next->size = (*after)->size - (size_with_header - length);
                        next->next = (*after)->next;
                        *after = next;
                    }
                }
            }
            start->size = length;
            return &start->bytes;
        } else {
            void* ret = kalloc(size);
            memmove(ret, ptr, mem->size - sizeof(AllocatedMemory));
            dealloc(ptr);
            return ret;
        }
        unlockSpinLock(&kalloc_lock);
    }
}

