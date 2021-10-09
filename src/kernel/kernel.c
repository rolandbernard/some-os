
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "error/log.h"
#include "devices/devices.h"
#include "interrupt/com.h"
#include "interrupt/syscall.h"
#include "interrupt/trap.h"
#include "process/harts.h"
#include "process/process.h"
#include "process/schedule.h"
#include "kernel/init.h"

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
    status = initVirtualFileSystem();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize filesystem: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Filesystem initialized");
    }

    // Enqueue main process to start the init process
    enqueueProcess(createKernelProcess(kernelMain, DEFAULT_PRIORITY, HART_STACK_SIZE));
    // Wake up the remaining harts
    sendMessageToAll(INITIALIZE_HARTS, NULL);
    // Init the timer interrupts
    initTimerInterrupt();
    // Start running the main process
    runNextProcess();
}

void kernelMain() {
    int res;
    // Start the init process
    res = syscall(SYSCALL_MOUNT, "/dev/blk0", "/", "minix", NULL); // Mount root filesystem
    if (res < 0) {
        KERNEL_LOG("[!] Failed to mount root filesystem: %s", getErrorKindMessage(-res));
    } else {
        res = syscall(SYSCALL_EXECVE, "/bin/init", NULL, NULL);
        // If we continue, there must be an error
        KERNEL_LOG("[!] Failed to start init process: %s", getErrorKindMessage(-res));
    }
    panic();
    syscall(SYSCALL_EXIT);
}

