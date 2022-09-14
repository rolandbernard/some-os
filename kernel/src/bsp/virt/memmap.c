
#include <stddef.h>

#include "bsp/virt/memmap.h"

const MemmapEntry memory_map[] = {
    [MEM_DEBUG]     = {        0x0,      0x100 },
    [MEM_MROM]      = {     0x1000,    0x11000 },
    [MEM_TEST]      = {   0x100000,     0x1000 },
    [MEM_CLINT]     = {  0x2000000,    0x10000 },
    [MEM_PCIE_PIO]  = {  0x3000000,    0x10000 },
    [MEM_PLIC]      = {  0xc000000,  0x4000000 },
    [MEM_UART0]     = { 0x10000000,      0x100 },
    [MEM_VIRTIO]    = { 0x10001000,     0x8000 },
    [MEM_PCIE_ECAM] = { 0x30000000, 0x10000000 },
    [MEM_PCIE_MMIO] = { 0x40000000, 0x40000000 },
    [MEM_DRAM]      = { 0x80000000,        0x0 },
};

