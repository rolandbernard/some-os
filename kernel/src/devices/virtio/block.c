
#include "devices/virtio/block.h"
#include "error/error.h"
#include "interrupt/plic.h"
#include "memory/kalloc.h"
#include "task/spinlock.h"

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
        return someError(EUNSUP, "Device does not support some features");
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
        callback(someError(EINVAL, "Read-only device write attempt"), udata);
    }
    lockSpinLock(&device->lock);
    if (device->virtio.queue->available.index - device->virtio.ack_index > VIRTIO_RING_SIZE) {
        unlockSpinLock(&device->lock);
        callback(someError(EBUSY, "Queue is full"), udata);
        return;
    }
    uint32_t sector = offset / BLOCK_SECTOR_SIZE;
    VirtIOBlockRequest* request = kalloc(sizeof(VirtIOBlockRequest));
    assert(request != NULL);
    request->header.sector = sector;
    request->header.reserved = 0;
    request->header.blk_type = write ? VIRTIO_BLOCK_T_OUT : VIRTIO_BLOCK_T_IN;
    request->data.data = buffer;
    request->status.status = VIRTIO_BLOCK_S_UNKNOWN;
    uint16_t head = addDescriptorsFor(&device->virtio, virtPtrForKernel(&request->header), sizeof(VirtIOBlockRequestHeader), VIRTIO_DESC_NEXT, true);
    addDescriptorsFor(&device->virtio, buffer, size, VIRTIO_DESC_NEXT | (write ? 0 : VIRTIO_DESC_WRITE), write);
    addDescriptorsFor(&device->virtio, virtPtrForKernel(&request->status), sizeof(VirtIOBlockRequestStatus), VIRTIO_DESC_WRITE, true);
    sendRequestAt(&device->virtio, head);
    request->head = head;
    request->callback = callback;
    request->udata = udata;
    request->next = device->requests;
    device->requests = request;
    unlockSpinLock(&device->lock);
}

void virtIOBlockFreePendingRequests(VirtIOBlockDevice* device) {
    VirtIOBlockRequest* requests = NULL;
    lockSpinLock(&device->lock);
    for (; device->virtio.ack_index != device->virtio.queue->used.index; device->virtio.ack_index++) {
        VirtIOUsedElement elem = device->virtio.queue->used.ring[device->virtio.ack_index % VIRTIO_RING_SIZE];
        VirtIOBlockRequest** current = &device->requests;
        while (*current != NULL) {
            if ((*current)->head == elem.id) {
                VirtIOBlockRequest* finished = *current;
                *current = finished->next;
                finished->next = requests;
                requests = finished;
                break;
            } else {
                current = &(*current)->next;
            }
        }
    }
    unlockSpinLock(&device->lock);
    while (requests != NULL) {
        VirtIOBlockRequest* request = requests;
        requests = request->next;
        Error error;
        switch (request->status.status) {
            case VIRTIO_BLOCK_S_OK:
                error = simpleError(SUCCESS);
                break;
            case VIRTIO_BLOCK_S_IOERR:
                error = simpleError(EIO);
                break;
            case VIRTIO_BLOCK_S_UNSUPP:
                error = simpleError(EUNSUP);
                break;
            case VIRTIO_BLOCK_S_UNKNOWN:
                error = simpleError(EIO);
                break;
        }
        request->callback(error, request->udata);
        dealloc(request);
    }
}

