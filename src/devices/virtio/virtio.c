
#include "devices/virtio/virtio.h"

#include "devices/virtio/block.h"
#include "memory/memmap.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"

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
            (VirtIODeviceLayout*)(memory_map[VIRT_VIRTIO].base + VIRTIO_MEM_STROBE * i);
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
    uint32_t max_queues = device->mmio->queue_num_max;
    if (max_queues < VIRTIO_RING_SIZE) {
        return someError(UNSUPPORTED, "Queue size maximum too low");
    }
    device->mmio->queue_num = VIRTIO_RING_SIZE;
    if (device->queue == NULL) {
        device->mmio->queue_sel = 0;
        size_t num_pages = (sizeof(VirtIOQueue) + PAGE_SIZE - 1) / PAGE_SIZE;
        VirtIOQueue* queue = zallocPages(num_pages).ptr;
        uint32_t queue_pfn = ((uintptr_t)queue) / PAGE_SIZE;
        device->mmio->guest_page_size = PAGE_SIZE;
        device->mmio->queue_pfn = queue_pfn;
        device->queue = queue;
    }
    return simpleError(SUCCESS);
}

uint16_t fillNextDescriptor(VirtIODevice* device, VirtIODescriptor descriptor) {
    device->index = (device->index + 1) % VIRTIO_RING_SIZE;
    device->queue->descriptors[device->index] = descriptor;
    if ((descriptor.flags & VIRTIO_DESC_NEXT) != 0) {
        device->queue->descriptors[device->index].next = (device->index + 1) & VIRTIO_RING_SIZE;
    }
    return device->index;
}

uint16_t addDescriptorsFor(VirtIODevice* device, VirtPtr buffer, size_t length, bool next, uint16_t* count_out) {
    size_t part_count = getVirtPtrParts(buffer, length, NULL, 0);
    VirtPtrBufferPart parts[part_count];
    getVirtPtrParts(buffer, length, parts, part_count);
    uint16_t ret = 0;
    for (size_t i = 0; i < part_count; i++) {
        VirtIODescriptor desc = {
            .address = (uintptr_t)parts[i].address,
            .length = parts[i].length,
            .flags = ((i + 1 < part_count) || next) ? VIRTIO_DESC_NEXT : 0,
            .next = 0,
        };
        uint16_t index = fillNextDescriptor(device, desc);
        if (i == 0) {
            ret = index;
        }
    }
    if (count_out != NULL) {
        *count_out = part_count;
    }
    return ret;
}

void sendRequestAt(VirtIODevice* device, uint16_t descriptor) {
    device->queue->available.ring[device->queue->available.index] = descriptor;
    device->queue->available.index = (device->queue->available.index + 1) % VIRTIO_RING_SIZE;
    memoryFence();
    device->mmio->queue_notify = 0;
}

