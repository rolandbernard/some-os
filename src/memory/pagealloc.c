
#include <stdint.h>

#include "memory/pagealloc.h"

extern uintptr_t __heap_start;
extern uintptr_t __heap_end;

FreePages initializeFreePages() {
    uintptr_t start = (__heap_start + PAGE_SIZE - 1) & -PAGE_SIZE;
    uintptr_t end = __heap_start & -PAGE_SIZE;
    size_t count = (end - start) / PAGE_SIZE;
    FreePage* first = (FreePage*)start;
    first->next = NULL;
    first->size = count;
    FreePages ret = {
        .first = first,
    };
    return ret;
}

void* allocateNewPage(FreePages* free_pages) {
    if (free_pages->first != NULL) {
        if (free_pages->first->size > 1) {
            void* page = free_pages->first;
            FreePage* moved = page + PAGE_SIZE;
            moved->size = free_pages->first->size;
            moved->next = free_pages->first->next;
            free_pages->first = moved;
            return page;
        } else {
            FreePage* page = free_pages->first;
            free_pages->first = page->next;
            return page;
        }
    } else {
        return NULL;
    }
}

void freePage(FreePages* free_pages, void* ptr) {
    if (ptr != NULL) {
        FreePage* page = ptr;
        page->size = 1;
        page->next = free_pages->first;
        free_pages->first = page;
    }
}

