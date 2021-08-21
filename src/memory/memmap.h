#ifndef _MEMMAP_H_
#define _MEMMAP_H_

#include <stdint.h>

typedef enum {
    END,
    VIRT_DEBUG,
    VIRT_MROM,
    VIRT_TEST,
    VIRT_CLINT,
    VIRT_PLIC,
    VIRT_UART0,
    VIRT_VIRTIO,
    VIRT_DRAM,
    VIRT_PCIE_MMIO,
    VIRT_PCIE_PIO,
    VIRT_PCIE_ECAM,
} MemmapType;

typedef struct {
    MemmapType type;
    uintptr_t base;
    uintptr_t size;
} MemmapEntry;

extern const MemmapEntry virt_memmap[];

const MemmapEntry* findMemmapFor(MemmapType type);

#endif
