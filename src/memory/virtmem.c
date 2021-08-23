
#include "memory/virtmem.h"

#include "memory/memmap.h"
#include "error/log.h"

extern void __text_start;
extern void __text_end;

extern void __data_start;
extern void __stack_top;

PageTable* kernel_page_table;
SpinLock kernel_page_table_lock;

static void identityMapMapedMemory(MemmapType type) {
    MemmapEntry entry = memory_map[type];
    mapPageRange(kernel_page_table, entry.base, entry.base + entry.size, entry.base, PAGE_ENTRY_READ | PAGE_ENTRY_WRITE);
}

Error initKernelVirtualMemory() {
    lockSpinLock(&kernel_page_table_lock);
    kernel_page_table = createPageTable();
    // Identity map all the memory. Only text is executable. Text is read only.
    mapPageRange(
        kernel_page_table, (uintptr_t)&__text_start, (uintptr_t)&__text_end,
        (uintptr_t)&__text_start, PAGE_ENTRY_AD_RX
    );
    mapPageRange(
        kernel_page_table, (uintptr_t)&__data_start, (uintptr_t)&__stack_top,
        (uintptr_t)&__data_start, PAGE_ENTRY_AD_RW
    );
    // Identity map UART
    identityMapMapedMemory(VIRT_UART0);
    setVirtualMemory(0, kernel_page_table, true);
    unlockSpinLock(&kernel_page_table_lock);
    return simpleError(SUCCESS);
}
