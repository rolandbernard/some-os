
#include <stddef.h>
#include <stdio.h>

#include "devices/devices.h"
#include "error/log.h"

void kernelMain() {
    // Neccesary for other devices
    initBaselineDevices();

    // Initialize the kernel
    logKernelMessage("[+] Kernel started");
    initDevices();
    logKernelMessage("[+] Kernel initialized");

    // TODO: replace with starting init process
    // Just some testing code
    for (int i = 1; i <= 10; i++) {
        logKernelMessage("Hello world! %0*i", i, i);
    }
}

