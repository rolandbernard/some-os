
#include "devices/virtio/virtio.h"

#include "devices/virtio/block.h"
#include "memory/memmap.h"

static VirtIODevice* devices[VIRTIO_DEVICE_COUNT];

Error initVirtIODevices() {
    // TODO: implement
    for (int i = 0; i < VIRTIO_DEVICE_COUNT; i++) {
        volatile VirtIODeviceLayout* address =
            (VirtIODeviceLayout*)(memory_map[VIRT_VIRTIO].base + memory_map[VIRT_VIRTIO].size * i);
        if (address->magic_value == VIRTIO_MAGIC_NUMBER && address->device_id != 0) {
            switch ((VirtIODeviceType)address->device_id) {
                case VIRTIO_BLOCK: {
                    Error error = initBlockDevice(i, address, devices + i);
                    if (isError(error)) {
                        KERNEL_LOG(
                            "[!] Failed to initialize VirtIO block device %i: %s", i,
                            getErrorMessage(error)
                        );
                    } else {
                        KERNEL_LOG("[>] Initialized VirtIO block device %i", i);
                    }
                } break;
                default: KERNEL_LOG("[>] Unknown VirtIO device %i", i); break;
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

