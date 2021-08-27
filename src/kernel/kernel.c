
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "devices/devices.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "process/process.h"
#include "process/schedule.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"

void userMain() {
    // TODO: replace with starting init process
    // Just some testing code
    // Throws a Instruction page fault now, because user is not allowed to access kernel memory.
    double test = 1.23456789;
    for (int i = 1; i <= 5; i++) {
        test *= test;
        syscall(0, "Hello world!");
    }
    syscall(1);
}

uint64_t user_stack[512];
Process user_process;

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

    initProcess(&user_process, (uintptr_t)(user_stack + 512), 0, (uintptr_t)userMain);
    enqueueProcess(&user_process);
}

