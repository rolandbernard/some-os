#ifndef _MEMMAP_H_
#define _MEMMAP_H_

#include <stdint.h>

typedef enum {
    MEM_DEBUG,
    MEM_MROM,
    MEM_TEST,
    MEM_CLINT,
    MEM_PLIC,
    MEM_UART0,
    MEM_VIRTIO,
    MEM_DRAM,
    MEM_PCIE_MMIO,
    MEM_PCIE_PIO,
    MEM_PCIE_ECAM,
} MemmapType;

typedef struct {
    uintptr_t base;
    uintptr_t size;
} MemmapEntry;

extern const MemmapEntry memory_map[];

#endif
