#ifndef _VIRTPTR_H_
#define _VIRTPTR_H_

#include <stddef.h>

#include "memory/pagetable.h"

// This includes utility functions for handling pointers to other memory spaces

typedef struct {
    uintptr_t address;
    PageTable* table;
} VirtPtr;

typedef struct {
    void* address;
    size_t offset;
    size_t length;
} VirtPtrBufferPart;

VirtPtr virtPtrForKernel(void* addr);

VirtPtr virtPtrFor(uintptr_t addr, PageTable* table);

// Parts are segments of the buffer in the same page
size_t getVirtPtrParts(VirtPtr addr, size_t length, VirtPtrBufferPart* parts, size_t max_parts);

void memcpyBetweenVirtPtr(VirtPtr dest, VirtPtr src, size_t n);

void memsetVirtPtr(VirtPtr dest, int byte, size_t n);

#endif
