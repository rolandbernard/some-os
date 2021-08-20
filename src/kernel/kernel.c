
#include <stddef.h>
#include <stdio.h>

#include "devices/devices.h"
#include "error/log.h"
#include "process/process.h"
#include "schedule/schedule.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"
#include "util/assert.h"

void userMain() {
    // TODO: replace with starting init process
    // Just some testing code
    for (int i = 1; i <= 10; i++) {
        syscall(0, "Hello world!");
    }
    syscall(1);
}

Process user_process;
uint64_t user_stack[512];

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

    initProcess(&user_process, user_stack, NULL, userMain);
    enqueueProcess(&user_process);
}

