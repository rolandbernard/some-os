
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "devices/devices.h"
#include "error/log.h"
#include "files/vfs/fs.h"
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
    // Initialize baseline devices
    Error status = initBaselineDevices();
    if (isError(status)) {
        KERNEL_ERROR("Failed to initialize baseline devices: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_SUCCESS("Kernel started");
    }
    // Initialize kernel systems
    status = initAllSystems();
    if (isError(status)) {
        KERNEL_ERROR("Failed to initialize kernel: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_SUCCESS("Kernel initialized");
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
    assert(getCurrentTask() != NULL);
    // Initialize devices
    Error status = initDevices();
    if (isError(status)) {
        KERNEL_ERROR("Failed to initialize devices: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_SUCCESS("Devices initialized");
    }
    // Initialize virtual filesystem
    status = vfsInit(&global_file_system);
    if (isError(status)) {
        KERNEL_ERROR("Failed to initialize filesystem: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_SUCCESS("Filesystem initialized");
    }
    // Mount device filesystem
    int res = syscall(SYSCALL_MOUNT, NULL, "/dev", "dev", NULL);
    if (isError(status)) {
        KERNEL_ERROR("Failed to mount device filesystem: %s", getErrorKindMessage(-res));
        panic();
    } else {
        KERNEL_SUCCESS("Mounted device filesystem at /dev");
    }
    // Mount root filesystem
    res = syscall(SYSCALL_MOUNT, "/dev/blk0", "/", "minix", NULL);
    if (res < 0) {
        KERNEL_ERROR("Failed to mount root filesystem: %s", getErrorKindMessage(-res));
        panic();
    } else {
        KERNEL_SUCCESS("Mounted root filesystem /dev/blk0 at /");
    }
    // Start the init process
    res = syscall(SYSCALL_EXECVE, "/bin/init", NULL, NULL);
    // If we continue, there must be an error
    KERNEL_ERROR("Failed to start init process: %s", getErrorKindMessage(-res));
    panic();
}

