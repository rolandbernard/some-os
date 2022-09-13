
#include <stdint.h>
#include <string.h>

#include "error/backtrace.h"
#include "memory/virtmem.h"
#include "kernel/kernel.h"

extern char __bss_start[];
extern char __bss_end[];

static void clearBss() {
    memset(&__bss_start, 0, __bss_end - __bss_start);
}

typedef enum {
    UNKNOWN_STATE = 0,
    UNINITIALIZED = 1,
    BOOTED = 2,
} BootState;

BootState boot_state = UNINITIALIZED;

void runtimeInit(uint8_t* dtb) {
    clearBss();
    boot_state = BOOTED;
    memoryFence();
#ifdef DEBUG
    initBacktrace();
#endif
    kernelInit(dtb);
}

