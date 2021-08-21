
#include "memory/virtmem.h"

#include "memory/memmap.h"
#include "error/log.h"

extern void __text_start;
extern void __text_end;

extern void __data_start;
extern void __stack_top;

PageTable kernel_page_table;

static void identityMapMapedMemory(MemmapType type) {
    MemmapEntry entry = memory_map[type];
    mapPageRange(&kernel_page_table, entry.base, entry.base + entry.size, entry.base, PAGE_ENTRY_READ | PAGE_ENTRY_WRITE);
}

Error initKernelVirtualMemory() {
    // Identity map all the memory. Only text is executable. Text is read only.
    mapPageRange(
        &kernel_page_table, (uintptr_t)&__text_start, (uintptr_t)&__text_end,
        (uintptr_t)&__text_start, PAGE_ENTRY_READ | PAGE_ENTRY_EXEC
    );
    mapPageRange(
        &kernel_page_table, (uintptr_t)&__data_start, (uintptr_t)&__stack_top,
        (uintptr_t)&__data_start, PAGE_ENTRY_READ | PAGE_ENTRY_WRITE
    );

    // Identity map UART
    identityMapMapedMemory(VIRT_UART0);

    return simpleError(SUCCESS);
}

