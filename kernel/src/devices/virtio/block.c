
#include "devices/virtio/block.h"

#include "devices/devices.h"
#include "error/error.h"
#include "interrupt/plic.h"
#include "memory/kalloc.h"
#include "task/schedule.h"
#include "task/spinlock.h"
#include "task/syscall.h"

static void handleInterrupt(ExternalInterrupt id, void* udata) {
    virtIOBlockFreePendingRequests((VirtIOBlockDevice*)udata);
}

Error initVirtIOBlockDevice(volatile VirtIODeviceLayout* base, ExternalInterrupt itr_id) {
#ifdef DEBUG
    VirtIOBlockDeviceLayout* mmio = (VirtIOBlockDeviceLayout*)base;
    assert(mmio->config.blk_size == BLOCK_SECTOR_SIZE); // TODO: actually use the data in mmio->config?
#endif
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
        return someError(ENOTSUP, "Device does not support some features");
    }
    CHECKED(setupVirtIOQueue(&device->virtio), dealloc(device));
    status |= VIRTIO_DRIVER_OK;
    base->status = status;
    setInterruptFunction(itr_id, handleInterrupt, device);
    setInterruptPriority(itr_id, 1);
    registerVirtIOBlockDevice(device);
    return simpleError(SUCCESS);
}

static void waitForBlockOperation(void* _, VirtIOBlockDevice* device, VirtIOBlockRequest* request) {
    request->wakeup->sched.wakeup_function = NULL;
    moveTaskToState(request->wakeup, WAITING);
    enqueueTask(request->wakeup);
    sendRequestAt(&device->virtio, request->head);
    unlockSpinLock(&device->lock);
    runNextTask();
}

Error virtIOBlockDeviceOperation(VirtIOBlockDevice* device, VirtPtr buffer, size_t offset, size_t size, bool write) {
    if (write && device->read_only) {
        return someError(EINVAL, "Read-only device write attempt");
    } else if (size % BLOCK_SECTOR_SIZE != 0 || offset % BLOCK_SECTOR_SIZE != 0) {
        return someError(EINVAL, "Misaligned block device io attempt");
    }
    VirtIOBlockRequest request;
    request.wakeup = criticalEnter();
    assert(request.wakeup != NULL);
    lockSpinLock(&device->lock);
    if (device->virtio.queue->available.index - device->virtio.ack_index >= VIRTIO_RING_SIZE) {
        unlockSpinLock(&device->lock);
        return someError(EBUSY, "Queue is full");
    }
    uint32_t sector = offset / BLOCK_SECTOR_SIZE;
    request.header.sector = sector;
    request.header.reserved = 0;
    request.header.blk_type = write ? VIRTIO_BLOCK_T_OUT : VIRTIO_BLOCK_T_IN;
    request.data.data = buffer;
    request.status.status = VIRTIO_BLOCK_S_UNKNOWN;
    request.head = addDescriptorsFor(&device->virtio, virtPtrForKernel(&request.header), sizeof(VirtIOBlockRequestHeader), VIRTIO_DESC_NEXT, true);
    addDescriptorsFor(&device->virtio, buffer, size, VIRTIO_DESC_NEXT | (write ? 0 : VIRTIO_DESC_WRITE), write);
    addDescriptorsFor(&device->virtio, virtPtrForKernel(&request.status), sizeof(VirtIOBlockRequestStatus), VIRTIO_DESC_WRITE, true);
    request.next = device->requests;
    device->requests = &request;
    if (saveToFrame(&request.wakeup->frame)) {
        callInHart((void*)waitForBlockOperation, device, &request);
    }
    return request.result;
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
        switch (request->status.status) {
            case VIRTIO_BLOCK_S_OK:
                request->result = simpleError(SUCCESS);
                break;
            case VIRTIO_BLOCK_S_IOERR:
                request->result = simpleError(EIO);
                break;
            case VIRTIO_BLOCK_S_UNSUPP:
                request->result = simpleError(ENOTSUP);
                break;
            case VIRTIO_BLOCK_S_UNKNOWN:
                request->result = simpleError(EIO);
                break;
        }
        awakenTask(request->wakeup);
        enqueueTask(request->wakeup);
    }
}

typedef struct {
    BlockDevice base;
    VirtIOBlockDevice* virtio_device;
} VirtIORegisteredBlockDevice;

static Error readFunction(VirtIORegisteredBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    return virtIOBlockDeviceOperation(dev->virtio_device, buff, offset, size, false);
}

static Error writeFunction(VirtIORegisteredBlockDevice* dev, VirtPtr buff, size_t offset, size_t size) {
    return virtIOBlockDeviceOperation(dev->virtio_device, buff, offset, size, true);
}

static const BlockDeviceFunctions funcs = {
    .read = (BlockDeviceReadFunction)readFunction,
    .write = (BlockDeviceWriteFunction)writeFunction,
};

Error registerVirtIOBlockDevice(VirtIOBlockDevice* dev) {
    VirtIORegisteredBlockDevice* reg = kalloc(sizeof(VirtIORegisteredBlockDevice));
    VirtIOBlockDeviceLayout* mmio = (VirtIOBlockDeviceLayout*)dev->virtio.mmio;
    reg->base.block_size = BLOCK_SECTOR_SIZE;
    reg->base.size = mmio->config.capacity * BLOCK_SECTOR_SIZE;
    reg->base.base.type = DEVICE_BLOCK;
    reg->base.base.name = "blk";
    reg->base.functions = &funcs;
    reg->virtio_device = dev;
    registerDevice((Device*)reg);
    return simpleError(SUCCESS);
}

