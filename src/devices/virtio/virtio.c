
#include "devices/virtio/virtio.h"

#include "devices/virtio/block.h"
#include "memory/memmap.h"

static VirtIODevice* devices[VIRTIO_DEVICE_COUNT];

typedef Error (*VirtIOInitFunction)(int id, volatile VirtIODeviceLayout* base, VirtIODevice** out);

typedef struct {
    VirtIOInitFunction init;
    const char* name;
} VirtIODeviceInitEntry;

static const VirtIODeviceInitEntry device_inits[VIRTIO_DEVICE_TYPE_END] = {
    [VIRTIO_BLOCK] = {
        .init = initBlockDevice,
        .name = "block",
    },
};

Error initVirtIODevices() {
    for (int i = 0; i < VIRTIO_DEVICE_COUNT; i++) {
        volatile VirtIODeviceLayout* address =
            (VirtIODeviceLayout*)(memory_map[VIRT_VIRTIO].base + memory_map[VIRT_VIRTIO].size * i);
        VirtIODeviceType type = address->device_id;
        if (address->magic_value == VIRTIO_MAGIC_NUMBER && type != 0) {
            if (type < VIRTIO_DEVICE_TYPE_END && device_inits[type].init != NULL) {
                Error error = device_inits[type].init(i, address, devices + i);
                if (isError(error)) {
                    KERNEL_LOG(
                        "[!] Failed to initialize VirtIO %s device %i: %s", device_inits[type].name, i,
                        getErrorMessage(error)
                    );
                } else {
                    devices[i]->type = address->device_id;
                    devices[i]->mmio = address;
                    KERNEL_LOG("[>] Initialized VirtIO %s device %i", device_inits[type].name, i);
                }
            } else {
                KERNEL_LOG("[>] Unknown VirtIO device %i", i);
            }
        }
    }
    return simpleError(SUCCESS);
}

VirtIODevice* getDeviceWithId(int id) {
    return devices[id];
}

VirtIODevice* getAnyDeviceOfType(VirtIODeviceType type) {
    for (int i = 0; i < VIRTIO_DEVICE_COUNT; i++) {
        if (devices[i] != NULL && devices[i]->type == type) {
            return devices[i];
        }
    }
    return NULL;
}

size_t getDeviceCountOfType(VirtIODeviceType type) {
    size_t count = 0;
    for (int i = 0; i < VIRTIO_DEVICE_COUNT; i++) {
        if (devices[i] != NULL && devices[i]->type == type) {
            count++;
        }
    }
    return count;
}

void getDevicesOfType(VirtIODeviceType type, VirtIODevice** output) {
    size_t index = 0;
    for (int i = 0; i < VIRTIO_DEVICE_COUNT; i++) {
        if (devices[i] != NULL && devices[i]->type == type) {
            output[index] = devices[i];
            index++;
        }
    }
}

Error setupVirtIOQueue(VirtIODevice* device) {
    // TODO: implement
    return simpleError(SUCCESS);
}

