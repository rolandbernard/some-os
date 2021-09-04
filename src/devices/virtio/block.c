
#include "devices/virtio/block.h"
#include "memory/kalloc.h"

Error initBlockDevice(int id, volatile VirtIODeviceLayout* base, VirtIODevice** output) {
    VirtIOBlockDevice* device = zalloc(sizeof(VirtIOBlockDevice));
    device->virtio.type = base->device_id;
    device->virtio.mmio = base;
    base->status = 0;
    VirtIOStatusField status = VIRTIO_ACKNOWLEDGE;
    base->status = status;
    status |= VIRTIO_DRIVER;
    base->status = status;
    VirtIOBlockFeatures host_features = base->host_features;
    VirtIOBlockFeatures guest_features = host_features & ~VIRTIO_BLOCK_F_RO;
    device->read_only = (host_features & VIRTIO_BLOCK_F_RO) != 0;
    base->guest_features = guest_features;
    status |= VIRTIO_FEATURES_OK;
    base->status = status;
    if (!(base->status & VIRTIO_FEATURES_OK)) {
        dealloc(device);
        return someError(UNSUPPORTED, "Device does not support some features");
    }
    CHECKED(setupVirtIOQueue(&device->virtio), dealloc(device));
    status |= VIRTIO_DRIVER_OK;
    base->status = status;
    *output = (VirtIODevice*)device;
    return simpleError(SUCCESS);
}

Error blockDeviceOperation(
    VirtIOBlockDevice* device, VirtPtr buffer, uint32_t offset, uint32_t size, bool write,
    VirtIOBlockCallback callback, void* udata
) {
    assert(size % BLOCK_SECTOR_SIZE == 0);
    if (write && device->read_only) {
        return someError(UNSUPPORTED, "Read-only device write attempt");
    }
    uint32_t sector = offset / BLOCK_SECTOR_SIZE;
    VirtIOBlockRequest* request = kalloc(sizeof(VirtIOBlockRequest));
    request->header.sector = sector;
    request->header.reserved = 0;
    request->header.blk_type = write ? VIRTIO_BLOCK_T_OUT : VIRTIO_BLOCK_T_IN;
    request->data.data = buffer;
    request->status.status = BLOCK_STATUS_MAGIC;
    uint16_t head = addDescriptorsFor(&device->virtio, virtPtrForKernel(request), sizeof(VirtIOBlockRequestHeader), VIRTIO_DESC_NEXT);
    addDescriptorsFor(&device->virtio, buffer, size, VIRTIO_DESC_NEXT | (write ? 0 : VIRTIO_DESC_WRITE));
    addDescriptorsFor(&device->virtio, virtPtrForKernel(&request->status), sizeof(VirtIOBlockRequestStatus), VIRTIO_DESC_WRITE);
    sendRequestAt(&device->virtio, head);
    request->head = head;
    request->callback = callback;
    request->udata = udata;
    device->requests[device->req_index] = request;
    device->req_index = (device->req_index + 1) % BLOCK_MAX_REQUESTS;
    return simpleError(SUCCESS);
}

void freePendingRequests(VirtIOBlockDevice* device) {
    while (device->virtio.ack_index != device->virtio.queue->used.index) {
        VirtIOUsedElement elem = device->virtio.queue->used.ring[device->virtio.ack_index];
        device->virtio.ack_index = (device->virtio.ack_index + 1) % VIRTIO_RING_SIZE;
        for (uint16_t i = device->ack_index; i != device->req_index; i = (i + 1) % BLOCK_MAX_REQUESTS) {
            if (device->requests[i] != NULL && device->requests[i]->head == elem.id) {
                device->requests[i]->callback(device->requests[i]->udata);
                dealloc(device->requests[i]);
                device->requests[i] = NULL;
                break;
            }
        }
        while (device->ack_index != device->req_index && device->requests[device->ack_index] == NULL) {
            device->ack_index = (device->ack_index + 1) % BLOCK_MAX_REQUESTS;
        }
    }
}

