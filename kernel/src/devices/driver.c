
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
        drivers = krealloc(drivers, drivers_capacity * sizeof(Driver*));
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

static Error initDriverForNode(DeviceTreeNode* node, DriverFlags mask) {
    if (node->driver == NULL) {
        Driver* driver = findDriverForNode(node);
        if (driver != NULL && (driver->flags & mask) == mask) {
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

static Error initForStdoutDevices(DeviceTreeNode* node, void* null) {
    CHECKED(initDriverForNode(node, DRIVER_FLAGS_STDOUT));
    if (getStdoutDevice() != NULL) {
        return simpleError(SUCCESS_EXIT);
    } else {
        return simpleError(SUCCESS);
    }
}

Error initDriversForStdoutDevice() {
    DeviceTreeNode* chosen = findNodeAtPath("/chosen");
    if (chosen != NULL) {
        const char* prop =
            readPropertyStringOrDefault(findNodeProperty(chosen, "stdout-path"), 0, NULL);
        if (prop != NULL) {
            DeviceTreeNode* stdout = findNodeAtPath(prop);
            initDriverForNode(stdout, false);
            if (getStdoutDevice() != NULL) {
                return simpleError(SUCCESS);
            }
            // This means the device is not supported. (Try finding another one)
        }
    }
    Error error = forAllDeviceTreeNodesDo(initForStdoutDevices, NULL);
    if (error.kind == SUCCESS_EXIT) {
        return simpleError(SUCCESS);
    } else if (isError(error)) {
        return error;
    } else {
        return simpleError(ENOSYS);
    }
}

static Error initForInterruptDevices(DeviceTreeNode* node, void* null) {
    return initDriverForNode(node, DRIVER_FLAGS_INTERRUPT);
}

Error initDriversForInterruptDevices() {
    return forAllDeviceTreeNodesDo(initForInterruptDevices, NULL);
}

static Error initForDevices(DeviceTreeNode* node, void* null) {
    return initDriverForNode(node, DRIVER_FLAGS_NONE);
}

Error initDriversForDeviceTreeNodes() {
    return forAllDeviceTreeNodesDo(initForDevices, NULL);
}

