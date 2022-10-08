
#include "devices/devices.h"
#include "memory/kalloc.h"

#include "devices/special/special.h"

static Error zeroReadFunction(CharDevice* dev, VirtPtr buffer, size_t size, size_t* read, bool block) {
    memsetVirtPtr(buffer, 0, size);
    *read = size;
    return simpleError(SUCCESS);
}

static Error nullWriteFunction(CharDevice* dev, VirtPtr buffer, size_t size, size_t* written) {
    return simpleError(SUCCESS);
}

static const CharDeviceFunctions funcs = {
    .read = zeroReadFunction,
    .write = nullWriteFunction,
};

Error registerZeroDevice() {
    CharDevice* dev = kalloc(sizeof(CharDevice));
    dev->base.type = DEVICE_CHAR;
    dev->base.name = "zero";
    dev->functions = &funcs;
    registerDevice((Device*)dev);
    return simpleError(SUCCESS);
}

