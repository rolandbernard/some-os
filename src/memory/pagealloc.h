#ifndef _PAGEALLOC_H_
#define _PAGEALLOC_H_

#include <stddef.h>

#define PAGE_SIZE 4096

typedef struct FreePage_s {
    size_t size; // Number of free pages after this one
    struct FreePage_s* next;
} FreePage;

typedef struct {
    FreePage* first;
} FreePages;

// Initialize the memory for page allocation.
void initPageAllocator();

// Allocate a new page. If no pages are left, return NULL.
void* allocPage();

// Free an allocated page. Freeing an allocated page is undefined behavior.
void freePage(void* page);

#endif
