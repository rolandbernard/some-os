#ifndef _PAGETABLE_H_
#define _PAGETABLE_H_

#include <stdint.h>
#include <stdbool.h>

#define PAGE_ENTRY_VALID    (1 << 0)
#define PAGE_ENTRY_READ     (1 << 1)
#define PAGE_ENTRY_WRITE    (1 << 2)
#define PAGE_ENTRY_EXEC     (1 << 3)
#define PAGE_ENTRY_USER     (1 << 4)
#define PAGE_ENTRY_GLOBAL   (1 << 5)
#define PAGE_ENTRY_ACCESSED (1 << 6)
#define PAGE_ENTRY_DIRTY    (1 << 7)

#define PAGE_ENTRY_RW (PAGE_ENTRY_READ | PAGE_ENTRY_WRITE)
#define PAGE_ENTRY_RX (PAGE_ENTRY_READ | PAGE_ENTRY_EXEC)
#define PAGE_ENTRY_AD (PAGE_ENTRY_ACCESSED | PAGE_ENTRY_DIRTY)

#define PAGE_ENTRY_AD_RW (PAGE_ENTRY_AD | PAGE_ENTRY_RW)
#define PAGE_ENTRY_AD_RX (PAGE_ENTRY_AD | PAGE_ENTRY_RX)

typedef union {
    uint64_t entry;
    struct {
        uint64_t bits : 10;
        uint64_t paddr : 44;
        uint64_t reserved0 : 10;
    };
    struct {
        bool v : 1;
        bool r : 1;
        bool w : 1;
        bool x : 1;
        bool u : 1;
        bool g : 1;
        bool a : 1;
        bool d : 1;
        uint64_t rsw : 2;
        uint64_t ppn0 : 9;
        uint64_t ppn1 : 9;
        uint64_t ppn2 : 26;
        uint64_t reserved1 : 10;
    };
} PageTableEntry;

#define PAGE_TABLE_SIZE 512

typedef struct {
    PageTableEntry entries[PAGE_TABLE_SIZE];
} PageTable;

// Allocate a new page table
PageTable* createPageTable();

// Free an allocated page table
void freePageTable(PageTable* table);

// Add a map to the given page table. This function should be idempotent. Returns the changes table
// entry.
PageTableEntry* mapPage(PageTable* root, uintptr_t vaddr, uintptr_t paddr, int bits, int level);

// Remove the map for a given virtual address. This function should be idempotent.
void unmapPage(PageTable* root, uintptr_t vaddr);

// Remove all maps from the given page root.
void unmapAllPages(PageTable* root);

// Remove all maps from the given page root and free pages if they are for the user.
void unmapAllPagesAndFreeUsers(PageTable* root);

// Map all pages between from_vaddr (inc) and to_vaddr (exc) to continuos addresses starting at paddr. Use the
// best fitting page level to do the mapping.
void mapPageRange(PageTable* root, uintptr_t from_vaddr, uintptr_t to_vaddr, uintptr_t paddr, int bits);

// Map all pages between from_vaddr (inc) and to_vaddr (exc) to continuos addresses starting at paddr. Use the
// given page level to do the mapping.
void mapPageRangeAtLevel(PageTable* root, uintptr_t from_vaddr, uintptr_t to_vaddr, uintptr_t paddr, int bits, int level);

// Remove all maps between from and to.
void unmapPageRange(PageTable* root, uintptr_t from, uintptr_t to);

// Copy all page mappings from dest to src and allocate new pages (and copy).
// Returns true if everything could be allocated, false otherwise.
// The destination table must be empty.
bool copyAllPagesAndAllocUsers(PageTable* dest, PageTable* src);

// Use the given page table to map from virtual to physical address. Returns 0 if unmapped.
uintptr_t virtToPhys(PageTable* root, uintptr_t vaddr);

// Get the entry handling the given vaddr in the given page table.
PageTableEntry* virtToEntry(PageTable* root, uintptr_t vaddr);

typedef void (*AllPagesDoCallback)(PageTableEntry* entry, uintptr_t vaddr, void* udata);

// Call the callback once for each leaf page
void allPagesDo(PageTable* root, AllPagesDoCallback callback, void* udata);

#endif
