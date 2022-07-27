
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "devices/devices.h"
#include "error/log.h"
#include "interrupt/com.h"
#include "interrupt/syscall.h"
#include "interrupt/trap.h"
#include "kernel/init.h"
#include "process/syscall.h"
#include "task/harts.h"
#include "task/schedule.h"
#include "task/task.h"

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
    // Wake up the remaining harts
    sendMessageToAll(INITIALIZE_HARTS, NULL);
    // Enqueue main process to start the init process
    enqueueTask(createKernelTask(kernelMain, HART_STACK_SIZE, DEFAULT_PRIORITY));
    // Init the timer interrupts
    initTimerInterrupt();
    // Start running the main process
    runNextTask();
}

void kernelMain() {
    Error status;
    // Initialize devices
    status = initDevices();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize devices: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Devices initialized");
    }
    // Initialize virtual filesystem
    status = initVirtualFileSystem();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize filesystem: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Filesystem initialized");
    }
    int res;
    // Mount filesystem
    res = syscall(SYSCALL_MOUNT, "/dev/blk0", "/", "minix", NULL); // Mount root filesystem
    if (res < 0) {
        KERNEL_LOG("[!] Failed to mount root filesystem: %s", getErrorKindMessage(-res));
        panic();
    }
    // Start the init process
    res = syscall(SYSCALL_EXECVE, "/bin/init", NULL, NULL);
    // If we continue, there must be an error
    KERNEL_LOG("[!] Failed to start init process: %s", getErrorKindMessage(-res));
    panic();
}

