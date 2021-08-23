#ifndef _KALLOC_H_
#define _KALLOC_H_

#include <stddef.h>

void* kalloc(size_t size);

void* zalloc(size_t size);

void dealloc(void* ptr);

void* krealloc(void* ptr, size_t size);

#endif
