
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "error/log.h"
#include "devices/devices.h"
#include "interrupt/syscall.h"
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

    enqueueProcess(createKernelProcess(kernelMain, DEFAULT_PRIORITY, HART_STACK_SIZE));
    runNextProcess();
}

char buff[2000000];

#define isOK(TEST) assert((int)(TEST) >= 0);

void kernelMain() {
    // Just some testing code
    isOK(syscall(SYSCALL_MOUNT, "dev/blk0", "", "minix", NULL));
    int fd = syscall(SYSCALL_OPEN, "test/test.txt", 0, 0);
    isOK(fd);
    long read = syscall(SYSCALL_READ, fd, buff, 100);
    isOK(read);
    buff[read] = 0;
    syscall(SYSCALL_PRINT, "READ: '");
    syscall(SYSCALL_PRINT, buff);
    syscall(SYSCALL_PRINT, "'\n");
    syscall(SYSCALL_CLOSE, fd);
    fd = syscall(SYSCALL_OPEN, "test/test2.txt", VFS_OPEN_CREATE, 600);
    isOK(fd);
    long written = syscall(SYSCALL_WRITE, fd, buff, read);
    isOK(written);
    long pos = syscall(SYSCALL_SEEK, fd, 0, VFS_SEEK_SET);
    isOK(pos);
    read = syscall(SYSCALL_READ, fd, buff + 100, 100);
    isOK(read);
    buff[100 + read] = 0;
    syscall(SYSCALL_PRINT, "READ: '");
    syscall(SYSCALL_PRINT, buff + 100);
    syscall(SYSCALL_PRINT, "'\n");
    syscall(SYSCALL_CLOSE, fd);
    // Just do some sleeping
    for (;;) {
        syscall(SYSCALL_PRINT, "Hello, sleeping...\n");
        syscall(SYSCALL_SLEEP, 1000000000UL);
    }
    syscall(SYSCALL_EXIT);
}

