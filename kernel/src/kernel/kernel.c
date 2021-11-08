
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
    // Wake up the remaining harts
    sendMessageToAll(INITIALIZE_HARTS, NULL);
    // Enqueue main process to start the init process
    enqueueProcess(createKernelProcess(kernelMain, DEFAULT_PRIORITY, HART_STACK_SIZE));
    // Init the timer interrupts
    initTimerInterrupt();
    // Start running the main process
    runNextProcess();
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
        syscall(SYSCALL_EXIT);
    }
    // Open file descriptors 0, 1 and 2
    for (int i = 0; i < 3; i++) {
        // 0 -> stdin, 1 -> stdout, 2 -> stderr
        res = syscall(
            SYSCALL_OPEN, "/dev/tty0",
            i == 0 ? VFS_OPEN_READ | VFS_OPEN_RDONLY : VFS_OPEN_WRITE | VFS_OPEN_WRONLY
        );
        if (res < 0) {
            KERNEL_LOG(
                "[!] Failed to open %s file: %s",
                i == 0   ? "stdin"
                : i == 1 ? "stdout"
                         : "stderr",
                getErrorKindMessage(-res)
            );
            panic();
            syscall(SYSCALL_EXIT);
        }
    }
    // Start the init process
    res = syscall(SYSCALL_EXECVE, "/bin/init", NULL, NULL);
    // If we continue, there must be an error
    KERNEL_LOG("[!] Failed to start init process: %s", getErrorKindMessage(-res));
    panic();
    syscall(SYSCALL_EXIT);
}
