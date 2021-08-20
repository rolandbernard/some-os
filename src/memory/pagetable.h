#ifndef _PAGETABLE_H_
#define _PAGETABLE_H_

#include <stdint.h>
#include <stdbool.h>

#define PAGE_TABLE_VALID    (1 << 0)
#define PAGE_TABLE_READ     (1 << 1)
#define PAGE_TABLE_WRITE    (1 << 2)
#define PAGE_TABLE_EXEC     (1 << 3)
#define PAGE_TABLE_USER     (1 << 4)
#define PAGE_TABLE_GLOBAL   (1 << 5)
#define PAGE_TABLE_ACCESSED (1 << 6)
#define PAGE_TABLE_DIRTY    (1 << 7)

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

typedef union {
    PageTableEntry entries[PAGE_TABLE_SIZE];
} PageTable;

void mapPage(PageTable* root, uintptr_t vaddr, uintptr_t paddr, uint64_t bits, uintptr_t level);

#endif
