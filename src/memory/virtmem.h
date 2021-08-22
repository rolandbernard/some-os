#ifndef _VIRTMEM_H_
#define _VIRTMEM_H_

#include "error/error.h"
#include "memory/pagetable.h"

extern PageTable* kernel_page_table;

Error initKernelVirtualMemory();

void setVirtualMemory(int asid, PageTable* page_table, bool fence);

#endif
