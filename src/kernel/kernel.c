
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
#include "process/process.h"
#include "process/schedule.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"

void kernelMain(int id) {
    // Just some testing code
    syscall(SYSCALL_PRINT, "KERNEL_ENTER %i\n", id);
    syscall(SYSCALL_YIELD);
    waitForInterrupt();
    syscall(SYSCALL_PRINT, "KERNEL_EXIT %i\n", id);
    syscall(SYSCALL_EXIT);
}

void kernelHigh() {
    // Just some testing code
    for (int i = 1;; i++) {
        syscall(SYSCALL_PRINT, "KERNEL_HIGH\n");
        syscall(SYSCALL_YIELD);
        waitForInterrupt();
    }
    syscall(SYSCALL_EXIT);
}

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
    // Initialize kernel systems
    status = initAllSystems();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize kernel: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Kernel initialized");
    }
    // Initialize devices
    status = initDevices();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize devices: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Devices initialized");
    }

    Process* process;
    for (int i = 0; i < MAX_PRIORITY; i++) {
        process = createKernelProcess(kernelMain, i, 1 << 16);
        process->frame.regs[9] = i;
        enqueueProcess(process);
    }
    process = createKernelProcess(kernelHigh, 0, 1 << 16);
    enqueueProcess(process);
    runNextProcess();
}

