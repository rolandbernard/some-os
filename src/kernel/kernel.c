
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "devices/devices.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "error/log.h"
#include "files/vfs.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"
#include "process/harts.h"
#include "process/process.h"
#include "process/schedule.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"
#include "process/types.h"

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
    // Initialize basic kernel systems
    status = initBasicSystems();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize kernel: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Kernel scheduler initialized");
    }

    enqueueProcess(createKernelProcess(kernelMain, DEFAULT_PRIORITY, HART_STACK_SIZE));
    runNextProcess();
}

void kernelMain() {
    Error status;
    // Initialize all kernel systems
    status = initAllSystems();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize kernel: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Kernel completely initialized");
    }
    // Initialize devices
    status = initDevices();
    if (isError(status)) {
        KERNEL_LOG("[!] Failed to initialize devices: %s", getErrorMessage(status));
        panic();
    } else {
        KERNEL_LOG("[+] Devices initialized");
    }
    testingCode();
}

#include "files/minix/minix.h"
#include "files/minix/maps.h"
#include "files/blkfile.h"

VfsFilesystem* fs;
VfsFile* file;
char buff[2000000];

void read8Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Read:  %i", read);
    buff[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
}

void trunc6Callback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 1000000, 0, read8Callback, buff);
}

void read7Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Read:  %i", read);
    buff[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
    file->functions->trunc(file, 0, 0, 0, trunc6Callback, NULL);
}

void trunc5Callback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 1000000, 0, read7Callback, buff);
}

void read6Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Read:  %i", read);
    buff[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
    file->functions->trunc(file, 0, 0, 1000000, trunc5Callback, NULL);
}

void trunc4Callback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 1000000, 0, read6Callback, buff);
}

void read5Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Read:  %i", read);
    buff[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
    file->functions->trunc(file, 0, 0, 100, trunc4Callback, NULL);
}

void trunc3Callback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 1000000, 0, read5Callback, buff);
}

void read4Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Read:  %i", read);
    buff[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
    file->functions->trunc(file, 0, 0, 100000, trunc3Callback, NULL);
}

void trunc2Callback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 1000000, 0, read4Callback, buff);
}

void read3Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Read:  %i", read);
    buff[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
    file->functions->trunc(file, 0, 0, 20000, trunc2Callback, NULL);
}

void truncCallback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 1000000, 0, read3Callback, buff);
}

void read2Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Read:  %i", read);
    buff[100] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
    file->functions->trunc(file, 0, 0, 42, truncCallback, NULL);
}

void writeCallback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 1000000, 0, read2Callback, NULL);
}

void read1Callback(Error error, size_t read, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    buff[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", buff);
    for (size_t i = 0; i < sizeof(buff); i++) {
        buff[i] = '0' + (i % 50);
    }
    vfsWriteAt(file, 0, 0, virtPtrForKernel(buff), 100, 0, writeCallback, NULL);
}

void chmodCallback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsReadAt(file, 0, 0, virtPtrForKernel(buff), 511, 0, read1Callback, NULL);
}

void chownCallback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    file->functions->chmod(file, 0, 0, 0711, chmodCallback, NULL);
}

void openCallback(Error error, VfsFile* f, void* udata) {
    file = f;
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] File:  %p", file);
    file->functions->chown(file, 0, 0, 70, 70, chownCallback, NULL);
}

void linkCallback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    vfsOpen(&global_file_system, 0, 0, "/test/test.txt", 0, 0, openCallback, NULL);
}

void initCallback(Error error, void* udata) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    mountFilesystem(&global_file_system, fs, "/"); 
    vfsLink(&global_file_system, 0, 0, "/test/test.txt", "/test/test2.txt", linkCallback, NULL);
    /* vfsUnlink(&global_file_system, 0, 0, "/test/test.txt", linkCallback, NULL); */
    /* vfsUnlink(&global_file_system, 0, 0, "/test/test.txt", linkCallback, NULL); */
}

void testingCode() {
    // Just some testing code
    VirtIOBlockDevice* device = (VirtIOBlockDevice*)getAnyDeviceOfType(VIRTIO_BLOCK);
    VirtIOBlockDeviceLayout* layout = (VirtIOBlockDeviceLayout*)device->virtio.mmio;
    BlockDeviceFile* file = createBlockDeviceFile(
        device, layout->config.blk_size, layout->config.capacity * layout->config.blk_size,
        (BlockOperationFunction)virtIOBlockDeviceOperation
    );
    fs = (VfsFilesystem*)createMinixFilesystem((VfsFile*)file, NULL);
    fs->functions->init(fs, 0, 0, initCallback, NULL);
    // Just do some sleeping
    for (uint64_t i = 0;; i++) {
        syscall(SYSCALL_PRINT, "Hello, sleeping for %is\n", i);
        syscall(SYSCALL_SLEEP, i * 1000000000UL);
    }
    syscall(SYSCALL_EXIT);
}


