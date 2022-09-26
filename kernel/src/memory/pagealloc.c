
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "memory/allocator.h"
#include "memory/pagealloc.h"
#include "memory/reclaim.h"
#include "task/spinlock.h"
#include "task/syscall.h"

static SpinLock alloc_lock;
static Allocator page_allocator = {
    .backing = NULL,
    .block_size = PAGE_SIZE,
    .first_free = NULL,
};

extern char __heap_start[];
extern char __heap_end[];

void* zero_page;

Error initPageAllocator() {
    uintptr_t start = ((uintptr_t)__heap_start + PAGE_SIZE - 1) & -PAGE_SIZE;
    uintptr_t end = (uintptr_t)__heap_end & -PAGE_SIZE;
    assert(end >= start);
    deallocMemory(&page_allocator, (void*)start, end - start);
    KERNEL_SUBSUCCESS("Initialized page allocator");
    zero_page = zallocPage();
    KERNEL_SUBSUCCESS("Initialized zero page");
    return simpleError(SUCCESS);
}

void* allocPage() {
    return allocPages(1).ptr;
}

static PageAllocation basicAllocPages(size_t pages) {
    PageAllocation ret = {
        .ptr = NULL,
        .size = 0,
    };
    if (pages != 0) {
        lockSpinLock(&alloc_lock);
        ret.ptr = allocMemory(&page_allocator, pages * PAGE_SIZE);
        if (ret.ptr != NULL) {
            ret.size = pages;
        }
        unlockSpinLock(&alloc_lock);
    }
    return ret;
}

PageAllocation allocPages(size_t pages) {
    PageAllocation alloc = basicAllocPages(pages);
    if (pages != 0 && alloc.size == 0) {
        // Try to reclaim memory.
        Priority priority = LOWEST_PRIORITY;
        while (tryReclaimingMemory(priority) || priority > HIGHEST_PRIORITY) {
            if (priority > HIGHEST_PRIORITY) {
                priority--;
            }
            alloc = basicAllocPages(pages);
            if (alloc.size > 0) {
                break;
            }
        }
    }
    return alloc;
}

void deallocPage(void* ptr) {
    PageAllocation alloc = {
        .ptr = ptr,
        .size = 1,
    };
    deallocPages(alloc);
}

void deallocPages(PageAllocation alloc) {
    if (alloc.ptr != NULL && alloc.size != 0) {
        // Small sanity checks to only free pages in the heap
        assert(alloc.ptr >= (void*)__heap_start && alloc.ptr <= (void*)__heap_end);
        assert(
            alloc.ptr + alloc.size * PAGE_SIZE >= (void*)__heap_start
            && alloc.ptr + alloc.size * PAGE_SIZE <= (void*)__heap_end
        );
        lockSpinLock(&alloc_lock);
        deallocMemory(&page_allocator, alloc.ptr, alloc.size * PAGE_SIZE);
        unlockSpinLock(&alloc_lock);
    }
}

void* zallocPage() {
    return zallocPages(1).ptr;
}

PageAllocation zallocPages(size_t pages) {
    PageAllocation alloc = allocPages(pages);
    memset(alloc.ptr, 0, alloc.size * PAGE_SIZE);
    return alloc;
}

