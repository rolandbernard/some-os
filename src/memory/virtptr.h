#ifndef _VIRTPTR_H_
#define _VIRTPTR_H_

#include <stddef.h>

#include "memory/pagetable.h"
#include "memory/memspace.h"

// This includes utility functions for handling pointers to other memory spaces

typedef struct {
    uintptr_t address;
    MemorySpace* table;
} VirtPtr;

typedef struct {
    void* address;
    size_t offset;
    size_t length;
} VirtPtrBufferPart;

VirtPtr virtPtrForKernel(void* addr);

VirtPtr virtPtrFor(uintptr_t addr, MemorySpace* mem);

// Parts are segments of the buffer in the same page
size_t getVirtPtrParts(VirtPtr addr, size_t length, VirtPtrBufferPart* parts, size_t max_parts, bool write);

typedef void* (*VirtPtrPartsDoCallback)(VirtPtrBufferPart part, void* udata);

void* virtPtrPartsDo(VirtPtr addr, size_t length, VirtPtrPartsDoCallback callback, void* udata, bool write);

void memcpyBetweenVirtPtr(VirtPtr dest, VirtPtr src, size_t n);

void memsetVirtPtr(VirtPtr dest, int byte, size_t n);

uint64_t readInt(VirtPtr addr, size_t size);

uint64_t readIntAt(VirtPtr addr, size_t i, size_t size);

void writeInt(VirtPtr addr, size_t size, uint64_t value);

void writeIntAt(VirtPtr addr, size_t size, size_t i, uint64_t value);

uint64_t readMisaligned(VirtPtr addr, size_t size);

void writeMisaligned(VirtPtr addr, size_t size, uint64_t value);

size_t strlenVirtPtr(VirtPtr str);

void strcpyVirtPtr(VirtPtr dest, VirtPtr src);

VirtPtr pushToVirtPtrStack(VirtPtr sp, void* ptr, size_t size);

VirtPtr popFromVirtPtrStack(VirtPtr sp, void* ptr, size_t size);

#define PUSH_TO_VIRTPTR(SP, OBJ) { SP = pushToVirtPtrStack(SP, &OBJ, sizeof(OBJ)); }

#define POP_FROM_VIRTPTR(SP, OBJ) { SP = popFromVirtPtrStack(SP, &OBJ, sizeof(OBJ)); }

#endif
