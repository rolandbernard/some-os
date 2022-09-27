
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "memory/kalloc.h"

#include "memory/allocator.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "task/spinlock.h"
#include "task/syscall.h"
#include "util/util.h"

#define KALLOC_MEM_ALIGN 8
#define KALLOC_MIN_FREE (32 * PAGE_SIZE)

typedef struct AllocatedMemory_s {
    size_t size; // Number of bytes allocated
#ifdef DEBUG
    struct AllocatedMemory_s* next;
#endif
    uint8_t bytes[];
} AllocatedMemory;

// This is a small buffer to be used for early allocation.
static FreeMemory small_buffer[1024] = { { .size = sizeof(small_buffer) } };

static SpinLock kalloc_lock;
Allocator byte_allocator = {
    .block_size = KALLOC_MEM_ALIGN,
    .min_backing_free = KALLOC_MIN_FREE,
    .backing = &page_allocator,
    .first_free = small_buffer,
    .special_range_start = small_buffer,
    .special_range_end = (void*)small_buffer + sizeof(small_buffer),
};

#ifdef DEBUG
static AllocatedMemory* allocated = NULL;
#endif

static size_t kallocActualSizeFor(size_t user_size) {
    return umax(
        (user_size + sizeof(AllocatedMemory) + KALLOC_MEM_ALIGN - 1) & -KALLOC_MEM_ALIGN,
        MINIMUM_FREE
    );
}

void* kalloc(size_t size) {
    if (size == 0) {
        return NULL;
    } else {
        size_t actual_size = kallocActualSizeFor(size);
        lockSpinLock(&kalloc_lock);
        AllocatedMemory* res = allocMemory(&byte_allocator, actual_size);
        unlockSpinLock(&kalloc_lock);
        if (res != NULL) {
            res->size = actual_size;
#ifdef DEBUG
            res->next = allocated;
            allocated = res;
#endif
            return res->bytes;
        } else {
            return NULL;
        }
    }
}

void* zalloc(size_t size) {
    void* mem = kalloc(size);
    if (mem != NULL) {
        memset(mem, 0, size);
    }
    return mem;
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
        AllocatedMemory* mem = findAllocatedMemoryFor(ptr, true);
        deallocMemory(&byte_allocator, mem, mem->size);
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
        size_t actual_size = kallocActualSizeFor(size);
        lockSpinLock(&kalloc_lock);
        AllocatedMemory* mem = findAllocatedMemoryFor(ptr, true);
        AllocatedMemory* res = reallocMemory(&byte_allocator, mem, mem->size, actual_size);
        unlockSpinLock(&kalloc_lock);
        if (res != NULL) {
            res->size = actual_size;
#ifdef DEBUG
            res->next = allocated;
            allocated = res;
#endif
            return res->bytes;
        } else {
            return NULL;
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

