
#include <assert.h>

#include "memory/pagetable.h"

#include "memory/pagealloc.h"

PageTable* createPageTable() {
    static_assert(PAGE_SIZE == sizeof(PageTable));
    return zallocPage();
}

void freePageTable(PageTable* table) {
    if (table != NULL) {
        unmapAllPages(table);
        deallocPage(table);
    }
}

void mapPage(PageTable* root, uintptr_t vaddr, uintptr_t paddr, int bits, int level) {
    // One of RWX bits must be set
    assert((bits & 0b1110) != 0);
    uintptr_t vpn[3] = {
        (vaddr >> 12) & 0x1ff,
        (vaddr >> 21) & 0x1ff,
        (vaddr >> 30) & 0x1ff,
    };
    PageTableEntry* entry = &root->entries[vpn[2]];
    for (int i = 1; i >= level; i--) {
        if (!entry->v) {
            PageTable* page = createPageTable();
            entry->entry = 0;
            entry->paddr = ((uintptr_t)page) >> 12;
            entry->v = true;
        }
        // This must not be a leaf node
        assert((entry->bits & 0b1110) == 0);
        PageTable* next_level = (PageTable*)(entry->paddr << 12);
        entry = &next_level->entries[vpn[i]];
    }
    // This must be a leaf node or invalid
    assert(!entry->v || (entry->bits & 0b1110) != 0);
    entry->paddr = paddr >> 12;
    entry->bits = bits;
    entry->v = true;
}

static bool tryToFreeTable(PageTable* table) {
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        PageTableEntry* entry = &table->entries[i];
        if (entry->v) {
            // Can not be freed
            return false;
        }
    }
    deallocPage(table);
    return true;
}

void unmapPage(PageTable* root, uintptr_t vaddr) {
    uintptr_t vpn[3] = {
        (vaddr >> 12) & 0x1ff,
        (vaddr >> 21) & 0x1ff,
        (vaddr >> 30) & 0x1ff,
    };
    PageTable* table[3];
    PageTableEntry* entry[3];
    table[2] = root;
    for (int i = 2; i >= 0; i--) {
        entry[i] = &table[i]->entries[vpn[i]];
        if (entry[i]->v) {
            if ((entry[i]->bits & 0b1110) == 0) {
                assert(i != 0); // Level 0 can not contain branches
                table[i - 1] = (PageTable*)(entry[i]->paddr << 12);
            } else {
                entry[i]->entry = 0;
                for (int j = i; j < 2; j++) {
                    if (tryToFreeTable(table[j])) {
                        entry[j + 1]->entry = 0;
                    } else {
                        break;
                    }
                }
                return;
            }
        } else {
            // The address is already unmapped
            return;
        }
    }
}

uintptr_t virtToPhys(PageTable* root, uintptr_t vaddr) {
    uintptr_t vpn[3] = {
        (vaddr >> 12) & 0x1ff,
        (vaddr >> 21) & 0x1ff,
        (vaddr >> 30) & 0x1ff,
    };
    PageTable* table = root;
    for (int i = 2; i >= 0; i--) {
        PageTableEntry* entry = &table->entries[vpn[i]];
        if (entry->v) {
            if ((entry->bits & 0b1110) == 0) {
                assert(i != 0); // Level 0 can not contain branches
                table = (PageTable*)(entry->paddr << 12);
            } else {
                uintptr_t mask = ((PAGE_SIZE << (9 * i)) - 1);
                uintptr_t offset = vaddr & mask;
                return ((entry->paddr << 12) & ~mask) | offset;
            }
        } else {
            // The address is unmapped
            return 0;
        }
    }
    return 0;
}

void unmapAllPages(PageTable* root) {
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        PageTableEntry* entry_lv2 = &root->entries[i];
        if (entry_lv2->v && (entry_lv2->bits & 0b1110) == 0) {
            // This is a non leaf node
            PageTable* table_lv1 = (PageTable*)(entry_lv2->paddr << 12);
            for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
                PageTableEntry* entry_lv1 = &table_lv1->entries[i];
                if (entry_lv1->v && (entry_lv1->bits & 0b1110) == 0) {
                    PageTable* table_lv0 = (PageTable*)(entry_lv1->paddr << 12);
                    // No more branches after level 0
                    deallocPage(table_lv0);
                }
            }
            deallocPage(table_lv1);
        }
        entry_lv2->entry = 0;
    }
}

void unmapAllPagesAndFreeUsers(PageTable* root) {
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        PageTableEntry* entry = &root->entries[i];
        if (entry->v) {
            if ((entry->bits & 0b1110) == 0) {
                PageTable* table = (PageTable*)(entry->paddr << 12);
                unmapAllPagesAndFreeUsers(table);
                deallocPage(table);
            } else if ((entry->bits & PAGE_ENTRY_USER) != 0) {
                void* page = (void*)(entry->paddr << 12);
                deallocPage(page);
            }
        }
        entry->entry = 0;
    }
}

// from_vaddr and to_vaddr should be aligned to page boundaries.
static void tryMapingRangeAtLevel(PageTable* root, uintptr_t from_vaddr, uintptr_t to_vaddr, uintptr_t paddr, int bits, int level) {
    if (from_vaddr != to_vaddr) {
        if (level == 0) {
            mapPageRangeAtLevel(root, from_vaddr, to_vaddr, paddr, bits, 0);
        } else {
            uintptr_t size = (PAGE_SIZE << (9 * level));
            uintptr_t start = (from_vaddr + size - 1) & -size;
            uintptr_t end = to_vaddr & -size;
            if (start < end && (end - start) > size) {
                tryMapingRangeAtLevel(root, from_vaddr, start, paddr, bits, level - 1);
                mapPageRangeAtLevel(root, start, end, paddr + (start - from_vaddr), bits, level);
                tryMapingRangeAtLevel(root, end, to_vaddr, paddr + (end - from_vaddr), bits, level - 1);
            } else {
                tryMapingRangeAtLevel(root, from_vaddr, to_vaddr, paddr, bits, level - 1);
            }
        }
    }
}

void mapPageRange(PageTable* root, uintptr_t from_vaddr, uintptr_t to_vaddr, uintptr_t paddr, int bits) {
    uintptr_t start = from_vaddr & -PAGE_SIZE;
    uintptr_t end = (to_vaddr + PAGE_SIZE - 1) & -PAGE_SIZE;
    tryMapingRangeAtLevel(root, start, end, paddr, bits, 2);
}

void mapPageRangeAtLevel(PageTable* root, uintptr_t from_vaddr, uintptr_t to_vaddr, uintptr_t paddr, int bits, int level) {
    uintptr_t size = (PAGE_SIZE << (9 * level));
    for (uintptr_t i = 0; i < to_vaddr - from_vaddr; i += size, paddr += size) {
        mapPage(root, from_vaddr + i, paddr, bits, level);
    }
}

void unmapPageRange(PageTable* root, uintptr_t from, uintptr_t to) {
    for (uintptr_t i = from; i < to; i += PAGE_SIZE) {
        unmapPage(root, i);
    }
}

