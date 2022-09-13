
#include "interrupt/clint.h"

#include "task/harts.h"

static uintptr_t clint_base_addr;

void sendMachineSoftwareInterrupt(int hart) {
    *(volatile uint32_t*)(clint_base_addr + hart * 0x4) = ~0;
}

void clearMachineSoftwareInterrupt(int hart) {
    *(volatile uint32_t*)(clint_base_addr + hart * 0x4) = 0;
}

void setTimeCmp(Time time) {
    *(volatile Time*)(clint_base_addr + 0x4000 + 8 * getCurrentHartId()) = time;
}

Time getTime() {
    return *(volatile Time*)(clint_base_addr + 0xbff8);
}

Error registerDriverClint() {
    return simpleError(ENOSYS);
}

