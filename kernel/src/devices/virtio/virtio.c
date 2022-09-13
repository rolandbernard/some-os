
#include "devices/virtio/virtio.h"

#include "devices/virtio/block.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"
#include "interrupt/plic.h"

Error initVirtIODevice(uintptr_t base_address, ExternalInterrupt itr_id) {
    volatile VirtIODeviceLayout* address = (VirtIODeviceLayout*)base_address;
    VirtIODeviceType type = address->device_id;
    if (address->magic_value == VIRTIO_MAGIC_NUMBER && type != 0) {
        if (type == VIRTIO_BLOCK) {
            Error error = initVirtIOBlockDevice(address, itr_id);
            if (isError(error)) {
                KERNEL_ERROR(
                    "Failed to initialize VirtIO block device at %p: %s",
                    base_address, getErrorMessage(error)
                );
                return error;
            } else {
                KERNEL_SUBSUCCESS("Initialized VirtIO block device %p", base_address);
                return simpleError(SUCCESS);
            }
        } else {
            KERNEL_WARNING("Unsupported VirtIO device type %i at %p", type, base_address);
            return simpleError(ENOTSUP);
        }
    } else {
        return someError(SUCCESS, "No device connected");
    }
}

Error setupVirtIOQueue(VirtIODevice* device) {
    if (device->queue == NULL) {
        uint32_t max_queues = device->mmio->queue_num_max;
        if (max_queues < VIRTIO_RING_SIZE) {
            return someError(ENOTSUP, "Queue size maximum too low");
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

Error registerDriverVirtIO() {
    return simpleError(ENOSYS);
}

