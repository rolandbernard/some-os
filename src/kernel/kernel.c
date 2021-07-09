
#include <stddef.h>
#include <stdio.h>

#include "devices/devices.h"
#include "error/log.h"

void kernelMain() {
    // Neccesary for other devices
    initBaselineDevices();

    logKernelMessage("[+] Kernel started");
    initDevices();
    logKernelMessage("[+] Kernel initialized");

    // Just some testing code
    logKernelMessage("Hello world!");
}

