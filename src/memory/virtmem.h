#ifndef _VIRTMEM_H_
#define _VIRTMEM_H_

#include "error/error.h"
#include "memory/pagetable.h"
#include "util/spinlock.h"

extern PageTable* kernel_page_table;

extern SpinLock kernel_page_table_lock;

Error initKernelVirtualMemory();

void setSatpCsr(uint64_t satp);

void addressTranslationFence(int asid);

void setVirtualMemory(uint16_t asid, PageTable* page_table, bool fence);

uint64_t satpForMemory(uint16_t asid, PageTable* page_table);

// This does not really belong here
void memoryFence();

#endif
