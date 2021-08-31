
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "memory/pagealloc.h"

extern void __heap_start;
extern void __heap_end;

static FreePages free_pages;

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
    return simpleError(SUCCESS);
}

void* allocPage() {
    return allocPages(1).ptr;
}

PageAllocation allocPages(size_t pages) {
    if (pages != 0) {
        FreePage** current = &free_pages.first;
        while (*current != NULL) {
            if ((*current)->size > pages) {
                FreePage* page = *current;
                FreePage* moved = page + PAGE_SIZE * pages;
                moved->size = page->size - pages;
                moved->next = page->next;
                *current = moved;
                PageAllocation ret = {
                    .ptr = page,
                    .size = pages,
                };
                return ret;
            } if ((*current)->size == pages) {
                FreePage* page = *current;
                *current = page->next;
                PageAllocation ret = {
                    .ptr = page,
                    .size = page->size,
                };
                return ret;
            } else {
                current = &(*current)->next;
            }
        }
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
        assert(alloc.ptr >= &__heap_start && alloc.ptr <= &__heap_end);
        if (alloc.ptr + PAGE_SIZE * alloc.size == free_pages.first) {
            FreePage* page = alloc.ptr;
            page->size = alloc.size + free_pages.first->size;
            page->next = free_pages.first->next;
            free_pages.first = page;
        } else if (((void*)free_pages.first) + PAGE_SIZE * free_pages.first->size == alloc.ptr) {
            free_pages.first->size += alloc.size;
        } else {
            FreePage* page = alloc.ptr;
            page->size = alloc.size;
            page->next = free_pages.first;
            free_pages.first = page;
        }
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

