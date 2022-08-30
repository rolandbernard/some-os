#ifndef _KALLOC_H_
#define _KALLOC_H_

#include <stddef.h>

void* kalloc(size_t size);

void* zalloc(size_t size);

void dealloc(void* ptr);

void* krealloc(void* ptr, size_t size);

// Returns the size of the allocation at the given pointer
size_t kallocSize(void* ptr);

#endif
