
#include <stdint.h>

#include "interrupt/com.h"

#include "memory/memmap.h"
#include "process/harts.h"
#include "process/schedule.h"

void sendMachineSoftwareInterrupt(int hart) {
    *(volatile uint32_t*)(memory_map[VIRT_CLINT].base + hart * 0x4) = 1;
}

void clearMachineSoftwareInterrupt(int hart) {
    *(volatile uint32_t*)(memory_map[VIRT_CLINT].base + hart * 0x4) = 0;
}

void handleMachineSoftwareInterrupt() {
    int hartid = getCurrentHartFrame()->hartid;
    clearMachineSoftwareInterrupt(hartid);
    // TODO: Actually handle it
    runNextProcess();
}

