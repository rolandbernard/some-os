
#include <string.h>

#include "interrupt/clint.h"

#include "devices/driver.h"
#include "memory/kalloc.h"
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

static bool checkDeviceCompatibility(const char* name) {
    return strstr(name, "clint") != NULL;
}

static Error initDeviceFor(DeviceTreeNode* node) {
    DeviceTreeProperty* reg = findNodeProperty(node, "reg");
    if (reg == NULL) {
        return simpleError(ENXIO);
    }
    clint_base_addr = readPropertyU64(reg, 0);
    return simpleError(SUCCESS);
}

Error registerDriverClint() {
    Driver* driver = kalloc(sizeof(Driver));
    driver->name = "riscv-clint";
    driver->flags = DRIVER_FLAGS_MMIO | DRIVER_FLAGS_INTERRUPT;
    driver->check = checkDeviceCompatibility;
    driver->init = initDeviceFor;
    registerDriver(driver);
    return simpleError(SUCCESS);
}

