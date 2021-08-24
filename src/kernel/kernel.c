
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "devices/devices.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "process/process.h"
#include "schedule/schedule.h"
#include "interrupt/syscall.h"
#include "kernel/init.h"

void userMain() {
    // TODO: replace with starting init process
    // Just some testing code
    // Throws a Instruction page fault now, because user is not allowed to access kernel memory.
    for (int i = 1; i <= 5; i++) {
        syscall(0, "Hello world!");
    }
    syscall(1);
}

Process user_process;
uint64_t user_stack[512];

void kernelMain() {
    Error status;

    // Initialize baseline devices
    status = initBaselineDevices();
    if (isError(status)) {
        KERNEL_LOG("[+] Failed to initialize baseline devices: %s", getErrorMessage(status));
    } else {
        KERNEL_LOG("[+] Kernel started");
    }

    // Initialize kernel systems
    status = initAllSystems();
    if (isError(status)) {
        KERNEL_LOG("[+] Failed to initialize kernel: %s", getErrorMessage(status));
    } else {
        KERNEL_LOG("[+] Kernel initialized");
    }

    // Initialize devices
    status = initDevices();
    if (isError(status)) {
        KERNEL_LOG("[+] Failed to initialize devices: %s", getErrorMessage(status));
    } else {
        KERNEL_LOG("[+] Devices initialized");
    }

    uint8_t** test = kalloc(20000 * sizeof(uint8_t*));
    size_t* length = kalloc(20000 * sizeof(size_t));
    for (int i = 0; i <= 20000; i++) {
        test[i] = NULL;
        length[i] = 0;
    }
    uint32_t seed = 12345;
    for (;;) {
        for (int t = 0; t < 10; t++) {
            for (int i = 0; i < 20000; i++) {
                if (i % 5000 == 0) {
                    logKernelMessage("%i - %i", t, i);
                }
                seed = (1103515245 * seed + 12345) % (1 << 31);
                for (size_t j = 0; j < length[i]; j++) {
                    assert(test[i][j] == i % 42);
                }
                length[i] = (seed >> 18) * 3 / 2;
                test[i] = krealloc(test[i], length[i]);
                memset(test[i], i % 42, length[i]);
            }
        }
        for (int i = 0; i <= 20000; i++) {
            dealloc(test[i]);
            test[i] = NULL;
            length[i] = 0;
        }
    }

    initProcess(&user_process, (uintptr_t)user_stack, 0, (uintptr_t)userMain);
    /* enqueueProcess(&user_process); */
}

