
#include <assert.h>
#include <string.h>

#include "memory/memspace.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "util/spinlock.h"

MemorySpace* createMemorySpace() {
    return createPageTable();
}

bool handlePageFault(MemorySpace* mem, uintptr_t address) {
    PageTableEntry* entry = virtToEntry(mem, address);
    if (entry != NULL && entry->v && (entry->bits & PAGE_ENTRY_COPY) != 0) {
        void* phy = (void*)(entry->paddr << 12);
        if (phy == zero_page) {
            void* page = zallocPage(); // Zero page, so zallocPage
            if (page == NULL) {
                return false;
            } else {
                entry->bits |= PAGE_ENTRY_WRITE;
                entry->bits &= ~PAGE_ENTRY_COPY;
                entry->paddr = (uint64_t)page >> 12;
                return true;
            }
        } else {
            // TODO: Check ref count and copy
            assert(false);
            return false;
        }
    } else {
        return false;
    }
}

uintptr_t virtToPhys(MemorySpace* mem, uintptr_t vaddr, bool write) {
    PageTableEntry* entry = virtToEntry(mem, vaddr);
    if (entry == NULL) {
        return 0;
    } else if (write) {
        if ((entry->bits & PAGE_ENTRY_WRITE) != 0) {
            return unsafeVirtToPhys(mem, vaddr);
        } else if ((entry->bits & PAGE_ENTRY_COPY) != 0) {
            if (handlePageFault(mem, vaddr)) {
                return (entry->paddr << 12);
            } else {
                // Page fault?
                return 0;
            }
        } else {
            return 0;
        }
    } else {
        if ((entry->bits & PAGE_ENTRY_READ) != 0) {
            return unsafeVirtToPhys(mem, vaddr);
        } else {
            return 0;
        }
    }
}

static void freePageEntryData(PageTableEntry* entry) {
    if ((entry->bits  & PAGE_ENTRY_USER) != 0) {
        void* phy = (void*)(entry->paddr << 12);
        if (phy != zero_page) {
            // TODO: check ref count table
            deallocPage((void*)(entry->paddr << 12));
        }
    }
}

void unmapAndFreePage(MemorySpace* mem, uintptr_t vaddr) {
    PageTableEntry* entry = virtToEntry(mem, vaddr);
    if (entry != NULL) {
        freePageEntryData(entry);
        entry->v = 0;
    }
}

static void freePagesInTable(PageTable* table, int level) {
    if (level >= 0) {
        for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
            PageTableEntry* entry = &table->entries[i];
            if (entry->v) {
                if ((entry->bits & 0b1110) == 0) {
                    PageTable* table = (PageTable*)(entry->paddr << 12);
                    freePagesInTable(table, level - 1);
                    deallocPage(table);
                } else {
                    freePageEntryData(entry);
                }
            }
            entry->entry = 0;
        }
    }
}

void freeMemorySpace(MemorySpace* mem) {
    freePagesInTable(mem, 2);
}

void deallocMemorySpace(MemorySpace* mem) {
    freeMemorySpace(mem);
    deallocPage(mem);
}

static bool copyMemoryPageEntry(PageTableEntry* dst, PageTableEntry* src) {
    if ((src->bits & PAGE_ENTRY_USER) != 0) {
        void* phy = (void*)(src->paddr << 12);
        if (phy != zero_page) {
            // TODO: check ref count table
            void* page = allocPage();
            if (page != NULL) {
                memcpy(page, phy, PAGE_SIZE);
                dst->paddr = (uintptr_t)page >> 12;
            } else {
                return false;
            }
        }
    }
    return true;
}

static bool copyAllPagesAndAllocUsers(PageTable* dest, PageTable* src) {
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        PageTableEntry* entry = &src->entries[i];
        dest->entries[i] = src->entries[i];
        if (entry->v) {
            if ((entry->bits & 0b1110) == 0) {
                PageTable* src_table = (PageTable*)(entry->paddr << 12);
                PageTable* dst_table = createPageTable();
                if (dst_table == NULL) {
                    return false;
                }
                if (!copyAllPagesAndAllocUsers(dst_table, src_table)) {
                    return false;
                }
                dest->entries[i].paddr = (uintptr_t)dst_table >> 12;
            } else {
                if (!copyMemoryPageEntry(&dest->entries[i], entry)) {
                    return false;
                }
            }
        }
    }
    return true;
}

MemorySpace* cloneMemorySpace(MemorySpace* mem) {
    MemorySpace* space = createMemorySpace();
    if (space != NULL) {
        if (!copyAllPagesAndAllocUsers(space, mem)) {
            deallocMemorySpace(space);
            space = NULL;
        }
    }
    return space;
}

