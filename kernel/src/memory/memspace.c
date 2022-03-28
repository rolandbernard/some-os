
#include <assert.h>
#include <string.h>

#include "memory/memspace.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/pageref.h"
#include "task/spinlock.h"

static SpinLock global_page_lock;
static PageRefTable ref_count;

MemorySpace* createMemorySpace() {
    return createPageTable();
}

bool handlePageFault(MemorySpace* mem, uintptr_t address) {
    PageTableEntry* entry = virtToEntry(mem, address);
    if (entry != NULL && entry->v && (entry->bits & PAGE_ENTRY_COPY) != 0) {
        // This is a copy-on-write page
        void* phy = (void*)((uintptr_t)entry->paddr << 12);
        if (phy == zero_page) {
            void* page = zallocPage(); // Zero page, so zallocPage
            if (page == NULL) {
                return false;
            } else {
                // We don't need to add references here, only if we have more than one reference
                entry->bits |= PAGE_ENTRY_WRITE;
                entry->bits &= ~PAGE_ENTRY_COPY;
                entry->paddr = (uintptr_t)page >> 12;
                return true;
            }
        } else {
            lockSpinLock(&global_page_lock);
            if (hasOtherReferences(&ref_count, (uintptr_t)phy)) {
                void* page = allocPage();
                if (page == NULL) {
                    // No more memory... Segfault!
                    unlockSpinLock(&global_page_lock);
                    return false;
                } else {
                    memcpy(page, phy, PAGE_SIZE);
                    entry->paddr = (uintptr_t)page >> 12;
                }
            } else {
                // If we have no other reference, we can reuse the current page
            }
            entry->bits |= PAGE_ENTRY_WRITE;
            entry->bits &= ~PAGE_ENTRY_COPY;
            unlockSpinLock(&global_page_lock);
            return true;
        }
    } else {
        return false;
    }
}

uintptr_t virtToPhys(MemorySpace* mem, uintptr_t vaddr, bool write, bool allow_all) {
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
                // Don't allow writing copy-on-write pages, even if allow_all_write is true
                // Page fault?
                return 0;
            }
        } else if (allow_all) {
            // Even if this page does not normally allow writing, write it anyways.
            // It is not a copy-on-write page, so everything should be ok.
            return unsafeVirtToPhys(mem, vaddr);
        } else {
            return 0;
        }
    } else {
        if ((entry->bits & PAGE_ENTRY_READ) != 0 || allow_all) {
            return unsafeVirtToPhys(mem, vaddr);
        } else {
            return 0;
        }
    }
}

static void freePageEntryData(PageTableEntry* entry) {
    if ((entry->bits  & PAGE_ENTRY_USER) != 0) {
        void* phy = (void*)((uintptr_t)entry->paddr << 12);
        if (phy != zero_page) {
            lockSpinLock(&global_page_lock);
            // Remove reference to the page we are freeing
            if (!hasOtherReferences(&ref_count, (uintptr_t)phy)) {
                unlockSpinLock(&global_page_lock);
                // If we have no other table using this page, deallocate it
                deallocPage(phy);
            } else {
                removeReferenceFor(&ref_count, (uintptr_t)phy);
                unlockSpinLock(&global_page_lock);
            }
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
                    PageTable* table = (PageTable*)((uintptr_t)entry->paddr << 12);
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
        void* phy = (void*)((uintptr_t)src->paddr << 12);
        if (phy != zero_page) {
            lockSpinLock(&global_page_lock);
            // We have at least two references, the one we copy from and the one we copyied.
            addReferenceFor(&ref_count, (uintptr_t)phy);
            unlockSpinLock(&global_page_lock);
            if ((src->bits & PAGE_ENTRY_WRITE) != 0) {
                // If this page can be written, we also have to set the copy-on-write flag
                src->bits |= PAGE_ENTRY_COPY;
                src->bits &= ~PAGE_ENTRY_WRITE;
                dst->bits |= PAGE_ENTRY_COPY;
                dst->bits &= ~PAGE_ENTRY_WRITE;
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
                PageTable* src_table = (PageTable*)((uintptr_t)entry->paddr << 12);
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

