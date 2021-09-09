
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "devices/devices.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "error/log.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "process/harts.h"
#include "process/process.h"
#include "process/schedule.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"
#include "process/types.h"

void kernelMain();
void testingCode();

void kernelInit() {
    Error status;
    // Initialize baseline devices
    status = initBaselineDevices();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize baseline devices: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Kernel started");
    }
    // Initialize basic kernel systems
    status = initBasicSystems();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize kernel: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Kernel scheduler initialized");
    }

    enqueueProcess(createKernelProcess(kernelMain, DEFAULT_PRIORITY, HART_STACK_SIZE));
    runNextProcess();
}

void kernelMain() {
    Error status;
    // Initialize all kernel systems
    status = initAllSystems();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize kernel: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Kernel completely initialized");
    }
    // Initialize devices
    status = initDevices();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize devices: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Devices initialized");
    }
    testingCode();
}

void testingCode() {
    // Just some testing code
    for (uint64_t i = 0;; i++) {
        syscall(SYSCALL_PRINT, "Hello, sleeping for %is\n", i);
        syscall(SYSCALL_SLEEP, i * 1000000000UL);
    }
    syscall(SYSCALL_EXIT);
}


