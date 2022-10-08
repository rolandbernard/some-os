
#include <string.h>

#include "devices/devices.h"

#include "devices/driver.h"
#include "devices/serial/uart16550.h"
#include "devices/special/null.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "memory/kalloc.h"
#include "util/text.h"

static SpinLock device_lock;
static DeviceId next_device_id = 1;
static size_t device_count = 0;
static size_t device_capacity = 0;
static Device** devices;

static size_t getDeviceNameId(const char* name) {
    size_t count = 0;
    for (size_t i = 0; i < device_count; i++) {
        if (strcmp(name, devices[i]->name) == 0 && devices[i]->name_id >= count) {
            count = devices[i]->name_id + 1;
        }
    }
    return count;
}

void registerDevice(Device* device) {
    lockSpinLock(&device_lock);
    device->id = next_device_id;
    next_device_id++;
    device->name_id = getDeviceNameId(device->name);
    if (device_count == device_capacity) {
        device_capacity = device_capacity == 0 ? 8 : device_capacity * 4 / 3;
        devices = krealloc(devices, device_capacity * sizeof(Device*));
    }
    devices[device_count] = device;
    device_count++;
    unlockSpinLock(&device_lock);
}

Device* getDeviceWithId(DeviceId id) {
    lockSpinLock(&device_lock);
    for (size_t i = 0; i < device_count; i++) {
        if (devices[i]->id == id) {
            unlockSpinLock(&device_lock);
            return devices[i];
        }
    }
    unlockSpinLock(&device_lock);
    return NULL;
}

Device* getDeviceNamed(const char* name, size_t name_id) {
    lockSpinLock(&device_lock);
    for (size_t i = 0; i < device_count; i++) {
        if (strcmp(name, devices[i]->name) == 0 && name_id == devices[i]->name_id) {
            unlockSpinLock(&device_lock);
            return devices[i];
        }
    }
    unlockSpinLock(&device_lock);
    return NULL;
}

Device* getNthDevice(size_t nth, bool* fst) {
    lockSpinLock(&device_lock);
    for (size_t i = 0; i < device_count; i++) {
        Device* dev = devices[i];
        if (nth == 0) {
            *fst = devices[i]->name_id == 0;
            unlockSpinLock(&device_lock);
            return dev;
        }
        nth--;
        if (devices[i]->name_id == 0) {
            if (nth == 0) {
                *fst = false;
                unlockSpinLock(&device_lock);
                return dev;
            }
            nth--;
        }
    }
    unlockSpinLock(&device_lock);
    return NULL;
}

CharDevice* getStdoutDevice() {
    return (CharDevice*)getDeviceNamed("tty", 0);
}

Error initStdoutDevice() {
    if (isError(initDriversForStdoutDevice())) {
        KERNEL_WARNING("Failed to find stdout in the device tree");
        // This should really not happen anymore
        return initBoardStdoutDevice();
    } else {
        return simpleError(SUCCESS);
    }
}

Error initInterruptDevice() {
    return initDriversForInterruptDevices();
}

Error initDevices() {
    CHECKED(registerNullDevice());
    return initDriversForDeviceTreeNodes();
}

