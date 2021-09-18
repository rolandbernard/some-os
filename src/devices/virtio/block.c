
#include "devices/virtio/block.h"
#include "error/error.h"
#include "interrupt/plic.h"
#include "memory/kalloc.h"
#include "util/spinlock.h"

static void handleInterrupt(ExternalInterrupt id, void* udata) {
    virtIOBlockFreePendingRequests((VirtIOBlockDevice*)udata);
}

Error initVirtIOBlockDevice(int id, volatile VirtIODeviceLayout* base, VirtIODevice** output) {
    VirtIOBlockDevice* device = zalloc(sizeof(VirtIOBlockDevice));
    assert(device != NULL);
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
    setInterruptFunction(id + 1, handleInterrupt, device);
    setInterruptPriority(id + 1, 1);
    return simpleError(SUCCESS);
}

void virtIOBlockDeviceOperation(
    VirtIOBlockDevice* device, VirtPtr buffer, uint32_t offset, uint32_t size, bool write,
    VirtIOBlockCallback callback, void* udata
) {
    assert(size % BLOCK_SECTOR_SIZE == 0);
    if (write && device->read_only) {
        callback(someError(UNSUPPORTED, "Read-only device write attempt"), udata);
    }
    lockSpinLock(&device->lock);
    if (
        device->virtio.ack_index == (device->virtio.queue->available.index + 1) % VIRTIO_RING_SIZE
        || device->ack_index == (device->req_index + 1) % BLOCK_MAX_REQUESTS
    ) {
        callback(someError(ALREADY_IN_USE, "Queue is full"), udata);
    }
    uint32_t sector = offset / BLOCK_SECTOR_SIZE;
    VirtIOBlockRequest* request = kalloc(sizeof(VirtIOBlockRequest));
    assert(request != NULL);
    request->header.sector = sector;
    request->header.reserved = 0;
    request->header.blk_type = write ? VIRTIO_BLOCK_T_OUT : VIRTIO_BLOCK_T_IN;
    request->data.data = buffer;
    request->status.status = VIRTIO_BLOCK_S_UNKNOWN;
    uint16_t head = addDescriptorsFor(&device->virtio, virtPtrForKernel(&request->header), sizeof(VirtIOBlockRequestHeader), VIRTIO_DESC_NEXT);
    addDescriptorsFor(&device->virtio, buffer, size, VIRTIO_DESC_NEXT | (write ? 0 : VIRTIO_DESC_WRITE));
    addDescriptorsFor(&device->virtio, virtPtrForKernel(&request->status), sizeof(VirtIOBlockRequestStatus), VIRTIO_DESC_WRITE);
    sendRequestAt(&device->virtio, head);
    request->head = head;
    request->callback = callback;
    request->udata = udata;
    device->requests[device->req_index] = request;
    device->req_index = (device->req_index + 1) % BLOCK_MAX_REQUESTS;
    unlockSpinLock(&device->lock);
}

void virtIOBlockFreePendingRequests(VirtIOBlockDevice* device) {
    lockSpinLock(&device->lock);
    while (device->virtio.ack_index != device->virtio.queue->used.index) {
        VirtIOUsedElement elem = device->virtio.queue->used.ring[device->virtio.ack_index];
        device->virtio.ack_index = (device->virtio.ack_index + 1) % VIRTIO_RING_SIZE;
        for (uint16_t i = device->ack_index; i != device->req_index; i = (i + 1) % BLOCK_MAX_REQUESTS) {
            if (device->requests[i] != NULL && device->requests[i]->head == elem.id) {
                VirtIOBlockRequest* request = device->requests[i];
                device->requests[i] = NULL;
                Error error;
                switch (request->status.status) {
                    case VIRTIO_BLOCK_S_OK:
                        error = simpleError(SUCCESS);
                        break;
                    case VIRTIO_BLOCK_S_IOERR:
                        error = simpleError(IO_ERROR);
                        break;
                    case VIRTIO_BLOCK_S_UNSUPP:
                        error = simpleError(UNSUPPORTED);
                        break;
                    case VIRTIO_BLOCK_S_UNKNOWN:
                        error = simpleError(UNKNOWN);
                        break;
                }
                request->callback(error, request->udata);
                dealloc(request);
                break;
            }
        }
        while (device->ack_index != device->req_index && device->requests[device->ack_index] == NULL) {
            device->ack_index = (device->ack_index + 1) % BLOCK_MAX_REQUESTS;
        }
    }
    unlockSpinLock(&device->lock);
}

