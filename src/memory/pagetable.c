
#include <assert.h>

#include "memory/pagealloc.h"
#include "memory/pagetable.h"

void mapPage(PageTable* root, uintptr_t vaddr, uintptr_t paddr, uint64_t bits, uintptr_t level) {
    // One of RWX bits must be set
    assert((bits & 0b1110) != 0);
    uintptr_t vpn[3] = {
        (vaddr >> 12) & 0x1ff,
        (vaddr >> 21) & 0x1ff,
        (vaddr >> 30) & 0x1ff,
    };
    PageTableEntry* entry = &root->entries[vpn[2]];
    for (int i = 1 - level; i >= 0; i--) {
        if (!entry->v) {
            void* page = callocPage();
            entry->entry = 0;
            entry->paddr = ((intptr_t)page) >> 12;
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

