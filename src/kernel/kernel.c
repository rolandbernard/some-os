
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "devices/devices.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "error/log.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "process/process.h"
#include "process/schedule.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"

void readCallback(VirtIOBlockStatus status, uint8_t* buffer) {
    assert(status == VIRTIO_BLOCK_S_OK);
    for (int i = 0; i < 128 / 2 / 8; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 8; k++) {
                logKernelMessage("%02x ", buffer[i * 16 + j * 8 + k]);
            }
            logKernelMessage(" ");
        }
        logKernelMessage("\n");
    }
}

void kernelMain() {
    // Just some testing code
    VirtIOBlockDevice* dev = (VirtIOBlockDevice*)getAnyDeviceOfType(VIRTIO_BLOCK);
    uint8_t buffer[512];
    blockDeviceOperation(dev, virtPtrForKernel(buffer), 0, 512, false, (VirtIOBlockCallback)readCallback, buffer);
    for (;;) {
        for (int i = 1; i <= 50; i++) {
            syscall(0, ".");
            waitForInterrupt();
        }
        syscall(0, "\n");
    }
    syscall(10);
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

    Process* process = createKernelProcess(kernelMain, 20);
    enqueueProcess(process);
    runNextProcess();
}

