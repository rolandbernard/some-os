
#include "devices/devices.h"

#include "devices/devfs.h"
#include "devices/serial/uart16550.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "memory/memmap.h"
#include "util/text.h"

extern Error initBoardBaselineDevices();
extern Error initBoardDevices();

static bool base_init = false;

#define MAX_DEVICE_COUNT 32

static SpinLock device_lock;
static DeviceId device_count = 0;
static Device* devices[MAX_DEVICE_COUNT];

Error initBaselineDevices() {
    CHECKED(initBoardBaselineDevices());
    base_init = true;
    return simpleError(SUCCESS);
}

Error initDevices() {
    CHECKED(initBoardDevices());
    CHECKED(initVirtIODevices());
    return simpleError(SUCCESS);
}

void registerDevice(Device* device) {
    lockSpinLock(&device_lock);
    devices[device_count] = device;
    device->id = device_count;
    device_count++;
    unlockSpinLock(&device_lock);
}

Device* getDeviceWithId(DeviceId id) {
    lockSpinLock(&device_lock);
    if (id < device_count) {
        Device* ret = devices[id];
        unlockSpinLock(&device_lock);
        return ret;
    } else {
        unlockSpinLock(&device_lock);
        return NULL;
    }
}

Device* getDeviceOfType(DeviceType type, DeviceId id) {
    while (id < device_count) {
        if (devices[id]->type == type) {
            return devices[id];
        }
        id++;
    }
    return NULL;
}

TtyDevice* getDefaultTtyDevice() {
    return (TtyDevice*)getDeviceOfType(DEVICE_TTY, 0);
}

Error mountDeviceFiles() {
    CHECKED(mountFilesystem(&global_file_system, (VfsFilesystem*)createDeviceFilesystem(), "/dev"))
    KERNEL_SUBSUCCESS("Mounted device filesystem at /dev");
    return simpleError(SUCCESS);
}

