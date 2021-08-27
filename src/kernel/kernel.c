
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "devices/devices.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "process/process.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"

void kernelMain() {
    Error status;
    // Initialize baseline devices
    status = initBaselineDevices();
    if (isError(status)) {
        KERNEL_LOG("[+] Failed to initialize baseline devices: %s", getErrorMessage(status));
    } else {
        KERNEL_LOG("[+] Kernel started");
    }
    // Initialize kernel systems
    status = initAllSystems();
    if (isError(status)) {
        KERNEL_LOG("[+] Failed to initialize kernel: %s", getErrorMessage(status));
    } else {
        KERNEL_LOG("[+] Kernel initialized");
    }
    // Initialize devices
    status = initDevices();
    if (isError(status)) {
        KERNEL_LOG("[+] Failed to initialize devices: %s", getErrorMessage(status));
    } else {
        KERNEL_LOG("[+] Devices initialized");
    }
}

