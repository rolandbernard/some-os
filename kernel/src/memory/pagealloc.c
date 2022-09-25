
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "memory/pagealloc.h"
#include "memory/reclaim.h"
#include "task/spinlock.h"
#include "task/syscall.h"

extern char __heap_start[];
extern char __heap_end[];

static SpinLock alloc_lock;
static FreePages free_pages;

void* zero_page;

Error initPageAllocator() {
    uintptr_t start = ((uintptr_t)__heap_start + PAGE_SIZE - 1) & -PAGE_SIZE;
    uintptr_t end = (uintptr_t)__heap_end & -PAGE_SIZE;
    assert(end >= start);
    size_t count = (end - start) / PAGE_SIZE;
    if (count > 0) {
        FreePage* first = (FreePage*)start;
        first->next = NULL;
        first->size = count;
        free_pages.first = first;
    } else {
        free_pages.first = NULL;
    }
    KERNEL_SUBSUCCESS("Initialized page allocator");
    zero_page = zallocPage();
    KERNEL_SUBSUCCESS("Initialized zero page");
    return simpleError(SUCCESS);
}

void* allocPage() {
    return allocPages(1).ptr;
}

static PageAllocation basicAllocPages(size_t pages) {
    if (pages != 0) {
        lockSpinLock(&alloc_lock);
        FreePage** current = &free_pages.first;
        while (*current != NULL) {
            if ((*current)->size > pages) {
                FreePage* page = *current;
                FreePage* moved = (FreePage*)((uintptr_t)page + PAGE_SIZE * pages);
                moved->size = page->size - pages;
                moved->next = page->next;
                *current = moved;
                PageAllocation ret = {
                    .ptr = page,
                    .size = pages,
                };
                unlockSpinLock(&alloc_lock);
                return ret;
            } else if ((*current)->size == pages) {
                FreePage* page = *current;
                *current = page->next;
                PageAllocation ret = {
                    .ptr = page,
                    .size = page->size,
                };
                unlockSpinLock(&alloc_lock);
                return ret;
            } else {
                current = &(*current)->next;
            }
        }
        unlockSpinLock(&alloc_lock);
    }
    PageAllocation ret = {
        .ptr = NULL,
        .size = 0,
    };
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
        FreePage** current = &free_pages.first;
#ifdef DEBUG
        // Small test to prevent double frees
        while (*current != NULL) {
            assert(
                alloc.ptr + alloc.size * PAGE_SIZE <= (void*)(*current)
                || alloc.ptr >= (void*)(*current) + (*current)->size * PAGE_SIZE
            );
            current = &(*current)->next;
        }
        current = &free_pages.first;
#endif
        while ((*current) != NULL && (void*)(*current) < alloc.ptr) {
            if ((void*)(*current) + (*current)->size * PAGE_SIZE == alloc.ptr) {
                alloc.ptr = (void*)(*current);
                alloc.size += (*current)->size;
                *current = (*current)->next;
            } else {
                current = &(*current)->next;
            }
        }
        FreePage* memory = alloc.ptr;
        memory->size = alloc.size;
        if ((*current) != NULL && alloc.ptr + alloc.size * PAGE_SIZE == (*current)) {
            memory->next = (*current)->next;
            memory->size += (*current)->size;
        } else {
            memory->next = (*current);
        }
        *current = memory;
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

