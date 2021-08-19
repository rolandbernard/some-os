
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
    if (free_pages.first != NULL) {
        if (free_pages.first->size > 1) {
            void* page = free_pages.first;
            FreePage* moved = page + PAGE_SIZE;
            moved->size = free_pages.first->size;
            moved->next = free_pages.first->next;
            free_pages.first = moved;
            return page;
        } else {
            FreePage* page = free_pages.first;
            free_pages.first = page->next;
            return page;
        }
    } else {
        return NULL;
    }
}

void freePage(void* ptr) {
    if (ptr != NULL) {
        if (ptr + PAGE_SIZE == free_pages.first) {
            FreePage* page = ptr;
            page->size = 1 + free_pages.first->size;
            page->next = free_pages.first->next;
            free_pages.first = page;
        } else if (((void*)free_pages.first) + PAGE_SIZE * free_pages.first->size == ptr) {
            free_pages.first->size += 1;
        } else {
            FreePage* page = ptr;
            page->size = 1;
            page->next = free_pages.first;
            free_pages.first = page;
        }
    }
}

