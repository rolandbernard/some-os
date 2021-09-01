
#include "devices/virtio/block.h"

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

