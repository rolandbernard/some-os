
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "memory/pagealloc.h"
#include "task/spinlock.h"

extern const void __heap_start;
extern const void __heap_end;

static SpinLock alloc_lock;
static FreePages free_pages;

void* zero_page;

Error initPageAllocator() {
    uintptr_t start = ((uintptr_t)&__heap_start + PAGE_SIZE - 1) & -PAGE_SIZE;
    uintptr_t end = (uintptr_t)&__heap_end & -PAGE_SIZE;
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
    KERNEL_LOG("[>] Initialized page allocator");
    zero_page = zallocPage();
    KERNEL_LOG("[>] Initialized zero page");
    return simpleError(SUCCESS);
}

void* allocPage() {
    return allocPages(1).ptr;
}

PageAllocation allocPages(size_t pages) {
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
    // TODO: handle memory pressure
    PageAllocation ret = {
        .ptr = NULL,
        .size = 0,
    };
    return ret;
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
        assert(alloc.ptr >= &__heap_start && alloc.ptr <= &__heap_end);
        assert(
            alloc.ptr + alloc.size * PAGE_SIZE >= &__heap_start
            && alloc.ptr + alloc.size * PAGE_SIZE <= &__heap_end
        );
        lockSpinLock(&alloc_lock);
        FreePage* memory = alloc.ptr;
        memory->next = NULL;
        memory->size = alloc.size;
        FreePage** current = &free_pages.first;
        while (*current != NULL) {
            uintptr_t current_ptr = (uintptr_t)*current;
            uintptr_t memory_ptr = (uintptr_t)memory;
            if (memory_ptr + memory->size * PAGE_SIZE == current_ptr) {
                memory->size += (*current)->size;
                *current = (*current)->next;
            } else if (current_ptr + (*current)->size * PAGE_SIZE == memory_ptr) {
                (*current)->size += memory->size;
                memory = *current;
                *current = (*current)->next;
            } else {
                current = &(*current)->next;
            }
        }
        memory->next = free_pages.first;
        free_pages.first = memory;
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

