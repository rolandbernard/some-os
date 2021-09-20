
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

VfsFile* file;
char buff[512];

void read2Callback(Error error, size_t read, char* string) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    string[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", string);
}

void writeCallback(Error error, size_t read, const char* string) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] Wrote:'%s'", string);
    vfsReadAt(
        file, 0, 0, virtPtrForKernel(buff), 511, 0, (VfsFunctionCallbackSizeT)read2Callback, buff
    );
}

void read1Callback(Error error, size_t read, char* string) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    string[read] = 0;
    KERNEL_LOG("[!] Read: '%s'", string);
    const char* test = "HELLO WORLD... TEST!";
    memcpy(buff, test, strlen(test) + 1);
    vfsWriteAt(
        file, 0, 0, virtPtrForKernel(buff), strlen(test), 0, (VfsFunctionCallbackSizeT)writeCallback, buff
    );
}

void openCallback(Error error, VfsFile* f, void* udata) {
    file = f;
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    KERNEL_LOG("[!] File:  %p", file);
    vfsReadAt(
        file, 0, 0, virtPtrForKernel(buff), 511, 0, (VfsFunctionCallbackSizeT)read1Callback, buff
    );
}

void initCallback(Error error, VfsFilesystem* fs) {
    KERNEL_LOG("[!] Error: %s", getErrorMessage(error));
    mountFilesystem(&global_file_system, fs, "/"); 
    vfsOpen(&global_file_system, 0, 0, "/test/test.txt", 0, 0, openCallback, fs);
}

void testingCode() {
    // Just some testing code
    VirtIOBlockDevice* device = (VirtIOBlockDevice*)getAnyDeviceOfType(VIRTIO_BLOCK);
    VirtIOBlockDeviceLayout* layout = (VirtIOBlockDeviceLayout*)device->virtio.mmio;
    BlockDeviceFile* file = createBlockDeviceFile(
        device, layout->config.blk_size, layout->config.capacity * layout->config.blk_size,
        (BlockOperationFunction)virtIOBlockDeviceOperation
    );
    MinixFilesystem* fs = createMinixFilesystem((VfsFile*)file, NULL);
    fs->base.functions->init((VfsFilesystem*)fs, 0, 0, (VfsFunctionCallbackVoid)initCallback, fs);
    // Just do some sleeping
    for (uint64_t i = 0;; i++) {
        syscall(SYSCALL_PRINT, "Hello, sleeping for %is\n", i);
        syscall(SYSCALL_SLEEP, i * 1000000000UL);
    }
    syscall(SYSCALL_EXIT);
}


