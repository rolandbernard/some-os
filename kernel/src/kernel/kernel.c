
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "devices/devices.h"
#include "devices/driver.h"
#include "error/log.h"
#include "files/vfs/fs.h"
#include "interrupt/com.h"
#include "interrupt/plic.h"
#include "interrupt/syscall.h"
#include "interrupt/trap.h"
#include "kernel/devtree.h"
#include "kernel/time.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "process/syscall.h"
#include "task/harts.h"
#include "task/schedule.h"
#include "task/task.h"

#define KERNEL_INIT_TASK(NAME, ACTION) {                            \
    Error status = ACTION;                                          \
    if (isError(status)) {                                          \
        KERNEL_ERROR(NAME ": failed, %s", getErrorMessage(status)); \
        panic();                                                    \
    } else {                                                        \
        KERNEL_SUCCESS(NAME ": success");                           \
    }                                                               \
}

Error initHart(int hartid) {
    setupHartFrame(hartid);
    initTraps();
    KERNEL_SUCCESS("Initialized hart %i", hartid);
    return simpleError(SUCCESS);
}

Error initPrimaryHart() {
    setupHartFrame(0);
    KERNEL_SUCCESS("Initialized hart 0");
    return simpleError(SUCCESS);
}

void kernelMain();

void kernelInit(uint8_t* dtb) {
    // Initialize the page level allocator. (Needed for allocation)
    KERNEL_INIT_TASK("Init page allocator", initPageAllocator());
    // Parse the device tree. (Needed for finding the output device)
    KERNEL_INIT_TASK("Initialize device tree", initDeviceTree(dtb));
    // Register drivers. (Needed for using the output device)
    KERNEL_INIT_TASK("Register drivers", registerAllDrivers());
    // Initialize interrupt devices. (Needed for using the other devices)
    KERNEL_INIT_TASK("Init interrupt controller", initInterruptDevice());
    // Initialize stdout device (Kernel logs will not work before the following)
    KERNEL_INIT_TASK("Init stdout device", initStdoutDevice());
    // Initialize the rest of the kernel
    KERNEL_INIT_TASK("Init kernel virtual memory", initKernelVirtualMemory());
    KERNEL_INIT_TASK("Init primary hart", initPrimaryHart());
    // Wake up the remaining harts
    sendMessageToAll(INITIALIZE_HARTS, NULL);
    // Enqueue main process to start the init process
    enqueueTask(createKernelTask(kernelMain, HART_STACK_SIZE, DEFAULT_PRIORITY));
    // Start running the main process
    runNextTask();
}

void kernelMain() {
    assert(getCurrentTask() != NULL);
    assert(getCurrentTask()->frame.hart != NULL);
    // Initialize devices
    KERNEL_INIT_TASK("Init devices", initDevices());
    // Initialize virtual filesystem
    KERNEL_INIT_TASK("Init vfs", vfsInit(&global_file_system));
    // Mount device filesystem
    int res = syscall(SYSCALL_MOUNT, NULL, "/dev", "dev", NULL);
    if (res != 0) {
        KERNEL_ERROR("Failed to mount device filesystem: %s", getErrorKindMessage(-res));
        panic();
    } else {
        KERNEL_SUCCESS("Mounted device filesystem at /dev");
    }
    // Mount root filesystem
    res = syscall(SYSCALL_MOUNT, "/dev/blk0", "/", "minix", NULL);
    if (res != 0) {
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

