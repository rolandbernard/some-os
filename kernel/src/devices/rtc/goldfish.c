
#include <string.h>

#include "devices/rtc/goldfish.h"

#include "devices/devices.h"
#include "devices/driver.h"
#include "interrupt/plic.h"
#include "memory/kalloc.h"
#include "task/spinlock.h"

#define	GOLDFISH_RTC_TIME_LOW	0x00
#define	GOLDFISH_RTC_TIME_HIGH	0x04

typedef struct {
    RtcDevice base;
    SpinLock lock;
    volatile uint8_t* base_address;
    ExternalInterrupt interrupt;
} GoldfishRtcDevice;

bool checkDeviceCompatibility(const char* str) {
    return strstr(str, "goldfish-rtc") != NULL;
}

static Error goldfishRtcGetTime(GoldfishRtcDevice* dev, Time* time) {
    lockSpinLock(&dev->lock);
    Time low = *(uint32_t*)(dev->base_address + GOLDFISH_RTC_TIME_LOW);
    Time high = *(uint32_t*)(dev->base_address + GOLDFISH_RTC_TIME_HIGH);
    *time = (high << 32) | low;
    unlockSpinLock(&dev->lock);
    return simpleError(SUCCESS);
}

static Error goldfishRtcSetTime(GoldfishRtcDevice* dev, Time time) {
    lockSpinLock(&dev->lock);
    *(uint32_t*)(dev->base_address + GOLDFISH_RTC_TIME_HIGH) = time >> 32;
    *(uint32_t*)(dev->base_address + GOLDFISH_RTC_TIME_LOW) = time & 0xffffffff;
    unlockSpinLock(&dev->lock);
    return simpleError(SUCCESS);
}

static const RtcDeviceFunctions funcs = {
    .get_time = (RtcDeviceGetTimeFunction)goldfishRtcGetTime,
    .set_time = (RtcDeviceSetTimeFunction)goldfishRtcSetTime,
};

Error initDeviceFor(DeviceTreeNode* node) {
    DeviceTreeProperty* reg = findNodeProperty(node, "reg");
    if (reg == NULL) {
        return simpleError(ENXIO);
    }
    GoldfishRtcDevice* dev = kalloc(sizeof(GoldfishRtcDevice));
    dev->base.base.type = DEVICE_RTC;
    dev->base.base.name = "rtc";
    dev->base.functions = &funcs;
    initSpinLock(&dev->lock);
    dev->base_address = (uint8_t*)readPropertyU64(reg, 0);
    dev->interrupt = readPropertyU32OrDefault(findNodeProperty(node, "interrupts"), 0, 0);
    registerDevice((Device*)dev);
    return simpleError(SUCCESS);
}

Error registerDriverGoldfishRtc() {
    Driver* driver = kalloc(sizeof(Driver));
    driver->name = "goldfish-rtc";
    driver->flags = DRIVER_FLAGS_MMIO;
    driver->check = checkDeviceCompatibility;
    driver->init = initDeviceFor;
    registerDriver(driver);
    return simpleError(SUCCESS);
}

