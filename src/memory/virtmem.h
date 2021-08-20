#ifndef _VIRTMEM_H_
#define _VIRTMEM_H_

#include <stdint.h>

typedef union {
    uint64_t entry;
    struct {
        uint64_t v : 1;
        uint64_t r : 1;
        uint64_t w : 1;
        uint64_t x : 1;
        uint64_t u : 1;
        uint64_t g : 1;
        uint64_t a : 1;
        uint64_t d : 1;
        uint64_t rsw : 2;
        uint64_t ppn0 : 9;
        uint64_t ppn1 : 9;
        uint64_t ppn2 : 26;
        uint64_t res : 10;
    };
} PageTableEntry;



#endif
