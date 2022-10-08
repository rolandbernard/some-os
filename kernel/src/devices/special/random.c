
#include "devices/devices.h"
#include "memory/kalloc.h"
#include "util/random.h"

#include "devices/special/special.h"

// Note: this is probably not secure, it is only here for compatibility.

static Error randReadFunction(CharDevice* dev, VirtPtr buffer, size_t size, size_t* read, bool block) {
    getRandom(buffer, size);
    *read = size;
    return simpleError(SUCCESS);
}

static Error nullWriteFunction(CharDevice* dev, VirtPtr buffer, size_t size, size_t* written) {
    return simpleError(SUCCESS);
}

static const CharDeviceFunctions funcs = {
    .read = randReadFunction,
    .write = nullWriteFunction,
};

Error registerRandomDevice() {
    CharDevice* dev = kalloc(sizeof(CharDevice));
    dev->base.type = DEVICE_CHAR;
    dev->base.name = "random";
    dev->functions = &funcs;
    registerDevice((Device*)dev);
    dev = kalloc(sizeof(CharDevice));
    dev->base.type = DEVICE_CHAR;
    dev->base.name = "urandom";
    dev->functions = &funcs;
    registerDevice((Device*)dev);
    return simpleError(SUCCESS);
}

