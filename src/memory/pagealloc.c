
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "memory/pagealloc.h"

extern uintptr_t __heap_start;
extern uintptr_t __heap_end;

static FreePages free_pages;

void initPageAllocator() {
    uintptr_t start = (__heap_start + PAGE_SIZE - 1) & -PAGE_SIZE;
    uintptr_t end = __heap_start & -PAGE_SIZE;
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
}

void* allocPage() {
    return allocPages(1).ptr;
}

PageAllocation allocPages(size_t max_pages) {
    assert(free_pages.first == NULL || free_pages.first->size != 0);
    if (free_pages.first != NULL && max_pages != 0) {
        if (free_pages.first->size > max_pages) {
            void* page = free_pages.first;
            FreePage* moved = page + PAGE_SIZE * max_pages;
            moved->size = free_pages.first->size - max_pages;
            moved->next = free_pages.first->next;
            free_pages.first = moved;
            PageAllocation ret = {
                .ptr = page,
                .size = max_pages,
            };
            return ret;
        } else {
            FreePage* page = free_pages.first;
            free_pages.first = page->next;
            PageAllocation ret = {
                .ptr = page,
                .size = page->size,
            };
            return ret;
        }
    } else {
        PageAllocation ret = {
            .ptr = NULL,
            .size = 0,
        };
        return ret;
    }
    assert(free_pages.first == NULL || free_pages.first->size != 0);
}

void deallocPage(void* ptr) {
    PageAllocation alloc = {
        .ptr = ptr,
        .size = 1,
    };
    deallocPages(alloc);
}

void deallocPages(PageAllocation alloc) {
    assert(free_pages.first == NULL || free_pages.first->size != 0);
    if (alloc.ptr != NULL && alloc.size != 0) {
        assert(alloc.ptr >= (void*)__heap_start && alloc.ptr <= (void*)__heap_end);
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
    assert(free_pages.first == NULL || free_pages.first->size != 0);
}

void* zallocPage() {
    return zallocPages(1).ptr;
}

PageAllocation zallocPages(size_t max_pages) {
    PageAllocation alloc = allocPages(max_pages);
    memset(alloc.ptr, 0, alloc.size * PAGE_SIZE);
    return alloc;
}

