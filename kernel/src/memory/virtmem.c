
#include "memory/virtmem.h"

#include "memory/memmap.h"
#include "error/log.h"

extern char __text_start[];
extern char __text_end[];

extern char __eh_frame_start[];
extern char __rodata_end[];

extern char __data_start[];
extern char __stack_top[];

PageTable* kernel_page_table;
SpinLock kernel_page_table_lock;

static void identityMapMapedMemory(MemmapType type) {
    MemmapEntry entry = memory_map[type];
    mapPageRange(kernel_page_table, entry.base, entry.base + entry.size, entry.base, PAGE_ENTRY_READ | PAGE_ENTRY_WRITE);
}

Error initKernelVirtualMemory() {
    lockSpinLock(&kernel_page_table_lock);
    kernel_page_table = createPageTable();
    // Identity map all the memory. Only .text is executable. .text and .rodata are read only.
    mapPageRange(
        kernel_page_table, (uintptr_t)__text_start, (uintptr_t)__text_end,
        (uintptr_t)__text_start, PAGE_ENTRY_AD_RX
    );
    mapPageRange(
        kernel_page_table, (uintptr_t)__eh_frame_start, (uintptr_t)__rodata_end,
        (uintptr_t)__eh_frame_start, PAGE_ENTRY_AD_R
    );
    mapPageRange(
        kernel_page_table, (uintptr_t)__data_start, (uintptr_t)__stack_top,
        (uintptr_t)__data_start, PAGE_ENTRY_AD_RW
    );
    // Identity map UART
    identityMapMapedMemory(VIRT_UART0);
    identityMapMapedMemory(VIRT_CLINT);
    identityMapMapedMemory(VIRT_PLIC);
    identityMapMapedMemory(VIRT_VIRTIO);
    setVirtualMemory(0, kernel_page_table);
    unlockSpinLock(&kernel_page_table_lock);
    KERNEL_LOG("[>] Initialized kernel virtual memory");
    return simpleError(SUCCESS);
}

void setVirtualMemory(uint16_t asid, PageTable* page_table) {
    setSatpCsr(satpForMemory(asid, page_table));
}

uint64_t satpForMemory(uint16_t asid, PageTable* page_table) {
    if (page_table == NULL) {
        return 0;
    } else {
        return 8L << 60 | (uint64_t)asid << 44L | (uintptr_t)page_table >> 12;
    }
}

