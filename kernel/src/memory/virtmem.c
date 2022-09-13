
#include "memory/virtmem.h"

#include "error/log.h"
#include "kernel/devtree.h"
#include "devices/driver.h"

extern char __text_start[];
extern char __text_end[];

extern char __eh_frame_start[];
extern char __rodata_end[];

extern char __data_start[];
extern char __stack_top[];

PageTable* kernel_page_table;
SpinLock kernel_page_table_lock;

static Error identityMapDeviceNode(DeviceTreeNode* node, void* null) {
    Driver* driver = findDriverForNode(node);
    if (driver != NULL && (driver->flags & DRIVER_FLAGS_MMIO) != 0) {
        DeviceTreeProperty* reg = findNodeProperty(node, "reg");
        if (reg == NULL) {
            return simpleError(SUCCESS);
        }
        for (size_t i = 0; 8 * i < reg->len; i += 2) {
            uintptr_t base = readPropertyU64(reg, i);
            uintptr_t size = readPropertyU64(reg, i + 1);
            mapPageRange(kernel_page_table, base, base + size, base, PAGE_ENTRY_READ | PAGE_ENTRY_WRITE);
        }
    }
    return simpleError(SUCCESS);
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
    // Identity map memory mapped devices for which we find a driver
    CHECKED(forAllDeviceTreeNodesDo(identityMapDeviceNode, NULL));
    setVirtualMemory(0, kernel_page_table);
    unlockSpinLock(&kernel_page_table_lock);
    KERNEL_SUBSUCCESS("Initialized kernel virtual memory");
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

