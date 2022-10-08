
#include "devices/devices.h"
#include "memory/kalloc.h"

#include "devices/special/null.h"

static Error nullReadFunction(CharDevice* dev, VirtPtr buffer, size_t size, size_t* read, bool block) {
    *read = 0;
    return simpleError(SUCCESS);
}

static Error nullWriteFunction(CharDevice* dev, VirtPtr buffer, size_t size, size_t* written) {
    return simpleError(SUCCESS);
}

static const CharDeviceFunctions funcs = {
    .read = nullReadFunction,
    .write = nullWriteFunction,
};

Error registerNullDevice() {
    CharDevice* dev = kalloc(sizeof(CharDevice));
    dev->base.type = DEVICE_CHAR;
    dev->base.name = "null";
    dev->functions = &funcs;
    registerDevice((Device*)dev);
    return simpleError(SUCCESS);
}

