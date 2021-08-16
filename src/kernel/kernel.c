
#include <stddef.h>
#include <stdio.h>

#include "devices/devices.h"
#include "error/log.h"

void kernelMain() {
    Error status;

    // Neccesary for other devices
    status = initBaselineDevices();
    if (isError(status)) {
        logKernelMessage("[+] Failed to initialize baseline devices: %s", getErrorMessage(status));
    } else {
        logKernelMessage("[+] Kernel started");
    }

    // Initialize the kernel
    status = initDevices();
    if (isError(status)) {
        logKernelMessage("[+] Failed to initialize devices: %s", getErrorMessage(status));
    } else {
        logKernelMessage("[+] Kernel initialized");
    }

    // TODO: replace with starting init process
    // Just some testing code
    for (int i = 1; i <= 10; i++) {
        logKernelMessage("Hello world! %0*i", i, i);
    }
}

