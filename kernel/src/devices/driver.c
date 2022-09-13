
#include "devices/driver.h"

#include "devices/serial/uart16550.h"
#include "devices/virtio/virtio.h"
#include "interrupt/clint.h"
#include "interrupt/plic.h"
#include "memory/kalloc.h"

static size_t drivers_count = 0;
static size_t drivers_capacity = 0;
static Driver** drivers;

Error registerAllDrivers() {
    CHECKED(registerDriverUart16550());
    CHECKED(registerDriverClint());
    CHECKED(registerDriverPlic());
    CHECKED(registerDriverVirtIO());
    return simpleError(SUCCESS);
}

void registerDriver(Driver* driver) {
    if (drivers_count == drivers_capacity) {
        drivers_capacity = drivers_capacity == 0 ? 8 : drivers_capacity * 4 / 3;
        drivers = krealloc(drivers, drivers_capacity);
    }
    drivers[drivers_count] = driver;
    drivers_count++;
}

static Driver* findDriverForCompat(const char* comp) {
    for (size_t i = 0; i < drivers_count; i++) {
        if (drivers[i]->check(comp)) {
            return drivers[i];
        }
    }
    return NULL;
}

Driver* findDriverForNode(DeviceTreeNode* node) {
    Driver* driver = NULL;
    DeviceTreeProperty* prop = findNodeProperty(node, "compatible");
    if (prop != NULL) {
        size_t n = 0;
        const char* comp = readPropertyString(prop, n);
        while (comp != NULL && driver == NULL) {
            driver = findDriverForCompat(comp);
            n++;
            comp = readPropertyString(prop, n);
        }
    }
    return driver;
}

static Error initDriverForNode(DeviceTreeNode* node, bool interrupt) {
    if (node->driver == NULL) {
        Driver* driver = findDriverForNode(node);
        if (driver != NULL && (!interrupt || (driver->flags & DRIVER_FLAGS_INTERRUPT) != 0)) {
            node->driver = driver;
            KERNEL_SUBSUCCESS("Using driver %s for device %s", driver->name, node->name);
            return driver->init(node);
        } else {
            return simpleError(SUCCESS);
        }
    } else {
        return simpleError(SUCCESS);
    }
}

Error initDriversForStdoutDevice() {
    return simpleError(ENOSYS);
}

static Error initForInterruptDevices(DeviceTreeNode* node, void* null) {
    return initDriverForNode(node, true);
}

Error initDriversForInterruptDevices() {
    return forAllDeviceTreeNodesDo(initForInterruptDevices, NULL);
}

static Error initForDevices(DeviceTreeNode* node, void* null) {
    return initDriverForNode(node, false);
}

Error initDriversForDeviceTreeNodes() {
    return forAllDeviceTreeNodesDo(initForDevices, NULL);
}

