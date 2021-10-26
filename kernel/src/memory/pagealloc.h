#ifndef _PAGEALLOC_H_
#define _PAGEALLOC_H_

#include <stddef.h>

#include "error/error.h"

#define PAGE_SIZE (1 << 12)

extern void* zero_page;

typedef struct FreePage_s {
    size_t size; // Number of free pages after this one
    struct FreePage_s* next;
} FreePage;

typedef struct {
    FreePage* first;
} FreePages;

typedef struct {
    void* ptr;
    size_t size;
} PageAllocation;

// Initialize the memory for page allocation.
Error initPageAllocator();

// Allocate a new page.
void* allocPage();

// Allocate a set of continuous pages. Allocate exactly max_pages or 0.
PageAllocation allocPages(size_t pages);

// Free an allocated page. Freeing an not allocated page is undefined behavior.
void deallocPage(void* page);

// Free a continuously allocated pages.
void deallocPages(PageAllocation allocation);

// Allocate a new page and fill the page with zeros.
void* zallocPage();

// Allocate a set of continuous pages and fill the page with zeros.
PageAllocation zallocPages(size_t pages);

#endif
