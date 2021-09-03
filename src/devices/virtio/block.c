
#include "devices/virtio/block.h"
#include "memory/kalloc.h"

static VirtIOBlockDevice devices[VIRTIO_DEVICE_COUNT];

Error initBlockDevice(int id, volatile VirtIODeviceLayout* base, VirtIODevice** output) {
    *output = (VirtIODevice*)&devices[id];
    devices[id].virtio.type = base->device_id;
    devices[id].virtio.mmio = base;
    base->status = 0;
    VirtIOStatusField status = VIRTIO_ACKNOWLEDGE;
    base->status = status;
    status |= VIRTIO_DRIVER_OK;
    base->status = status;
    VirtIOBlockFeatures host_features = base->host_features;
    VirtIOBlockFeatures guest_features = host_features & ~VIRTIO_BLOCK_F_RO;
    devices[id].read_only = (host_features & VIRTIO_BLOCK_F_RO) != 0;
    base->guest_features = guest_features;
    status |= VIRTIO_FEATURES_OK;
    base->status = status;
    if (!(base->status & VIRTIO_FEATURES_OK)) {
        return someError(UNSUPPORTED, "Device does not support some features");
    }
    CHECKED(setupVirtIOQueue(*output));
    status |= VIRTIO_DRIVER_OK;
    base->status = status;
    return simpleError(SUCCESS);
}

Error blockDeviceOperation(VirtIOBlockDevice* device, VirtPtr buffer, uint32_t offset, uint32_t size, bool write) {
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
    uint16_t head = addDescriptorsFor(&device->virtio, virtPtrForKernel(request), sizeof(VirtIOBlockRequestHeader), true, NULL);
    addDescriptorsFor(&device->virtio, buffer, size, true, NULL);
    addDescriptorsFor(&device->virtio, virtPtrForKernel(&request->status), sizeof(VirtIOBlockRequestStatus), false, NULL);
    sendRequestAt(&device->virtio, head);
    request->head = head;
    device->requests[device->req_index] = request;
    device->req_index = (device->req_index + 1) % BLOCK_MAX_REQUESTS;
    return simpleError(SUCCESS);
}



