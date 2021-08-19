
#include <stdint.h>

#include "memory/pagealloc.h"

extern uintptr_t __heap_start;
extern uintptr_t __heap_end;

static FreePages free_pages;

void initPageAllocator() {
    uintptr_t start = (__heap_start + PAGE_SIZE - 1) & -PAGE_SIZE;
    uintptr_t end = __heap_start & -PAGE_SIZE;
    size_t count = (end - start) / PAGE_SIZE;
    FreePage* first = (FreePage*)start;
    first->next = NULL;
    first->size = count;
    free_pages.first = first;
}

void* allocPage() {
    return allocPages(1).ptr;
}

void freePage(void* ptr) {
    PageAllocation alloc = {
        .ptr = ptr,
        .size = 1,
    };
    freePages(alloc);
}

PageAllocation allocPages(size_t max_pages) {
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
}

void freePages(PageAllocation alloc) {
    if (alloc.ptr != NULL && alloc.size != 0) {
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

