
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

    uint8_t* test[50];
    size_t length[50];
    for (int i = 0; i <= 50; i++) {
        test[i] = NULL;
        length[i] = 0;
    }
    uint32_t seed = 12345;
    for (;;) {
        for (int i = 0; i < 50; i++) {
            seed = (1664525 * seed + 1013904223);
            KERNEL_LOG("seed: %x", seed & 0x00ffffff);
            for (size_t j = 0; j < length[i]; j++) {
                assert(test[i][j] == i);
            }
            if (seed & 0b00100) {
                KERNEL_LOG("free(%p)", test[i]);
                dealloc(test[i]);
                test[i] = NULL;
                length[i] = 0;
            } else {
                KERNEL_LOG("realloc(%p, %u)", test[i], seed >> 16);
                length[i] = seed >> 16;
                test[i] = krealloc(test[i], length[i]);
                memset(test[i], i, length[i]);
            }
        }
    }

    initProcess(&user_process, (uintptr_t)user_stack, 0, (uintptr_t)userMain);
    /* enqueueProcess(&user_process); */
}

