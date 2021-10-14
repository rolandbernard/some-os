#ifndef _COPY_H_
#define _COPY_H_

#include <stdint.h>
#include <stdbool.h>

#include "memory/pagetable.h"

typedef PageTable MemorySpace;

MemorySpace* createMemorySpace();

// Return true if handled successfully
bool handlePageFault(MemorySpace* mem, uintptr_t address);

uintptr_t virtToPhys(MemorySpace* mem, uintptr_t vaddr, bool write);

void unmapAndFreePage(MemorySpace* mem, uintptr_t vaddr);

void freeMemorySpace(MemorySpace* mem);

void deallocMemorySpace(MemorySpace* mem);

MemorySpace* cloneMemorySpace(MemorySpace* mem);

#endif
