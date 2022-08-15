
#include "devices/virtio/virtio.h"

#include "devices/virtio/block.h"
#include "memory/memmap.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"

static VirtIODevice* devices[VIRTIO_DEVICE_COUNT];

typedef Error (*VirtIOInitFunction)(int id, volatile VirtIODeviceLayout* base, VirtIODevice** out);
typedef Error (*VirtIORegisterFunction)(VirtIODevice* dev);

typedef struct {
    VirtIOInitFunction init;
    const char* name;
} VirtIODeviceInitEntry;

static const VirtIODeviceInitEntry device_inits[VIRTIO_DEVICE_TYPE_END] = {
    [VIRTIO_BLOCK] = {
        .init = initVirtIOBlockDevice,
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
                    KERNEL_ERROR(
                        "Failed to initialize VirtIO %s device %i: %s", device_inits[type].name, i,
                        getErrorMessage(error)
                    );
                } else {
                    KERNEL_SUBSUCCESS("Initialized VirtIO %s device %i", device_inits[type].name, i);
                }
            } else {
                KERNEL_WARNING("Unknown VirtIO device %i", i);
            }
        }
    }
    return simpleError(SUCCESS);
}

Error setupVirtIOQueue(VirtIODevice* device) {
    if (device->queue == NULL) {
        uint32_t max_queues = device->mmio->queue_num_max;
        if (max_queues < VIRTIO_RING_SIZE) {
            return someError(EUNSUP, "Queue size maximum too low");
        }
        device->mmio->queue_num = VIRTIO_RING_SIZE;
        device->mmio->queue_sel = 0;
        size_t num_pages = (sizeof(VirtIOQueue) + PAGE_SIZE - 1) / PAGE_SIZE;
        VirtIOQueue* queue = zallocPages(num_pages).ptr;
        assert(queue != NULL);
        uint64_t queue_pfn = ((uintptr_t)queue) / PAGE_SIZE;
        device->mmio->guest_page_size = PAGE_SIZE;
        device->mmio->queue_pfn = queue_pfn;
        device->queue = queue;
    }
    return simpleError(SUCCESS);
}

uint16_t fillNextDescriptor(VirtIODevice* device, VirtIODescriptor descriptor) {
    device->desc_index = (device->desc_index + 1) % VIRTIO_RING_SIZE;
    device->queue->descriptors[device->desc_index] = descriptor;
    if ((descriptor.flags & VIRTIO_DESC_NEXT) != 0) {
        device->queue->descriptors[device->desc_index].next = (device->desc_index + 1) % VIRTIO_RING_SIZE;
    }
    return device->desc_index;
}

uint16_t addDescriptorsFor(VirtIODevice* device, VirtPtr buffer, size_t length, VirtIODescriptorFlags flags, bool write) {
    size_t part_count = getVirtPtrParts(buffer, length, NULL, 0, write);
    VirtPtrBufferPart parts[part_count];
    getVirtPtrParts(buffer, length, parts, part_count, write);
    uint16_t first_id = 0;
    for (size_t i = 0; i < part_count; i++) {
        VirtIODescriptor desc = {
            .address = (uintptr_t)parts[i].address,
            .length = parts[i].length,
            .flags = flags | ((i + 1 < part_count) ? VIRTIO_DESC_NEXT : 0),
            .next = 0,
        };
        uint16_t index = fillNextDescriptor(device, desc);
        if (i == 0) {
            first_id = index;
        }
    }
    return first_id;
}

void sendRequestAt(VirtIODevice* device, uint16_t descriptor) {
    device->queue->available.ring[device->queue->available.index % VIRTIO_RING_SIZE] = descriptor;
    device->queue->available.index++;
    memoryFence();
    device->mmio->queue_notify = 0;
}

