#ifndef _BLOCK_H_
#define _BLOCK_H_

#include "devices/virtio/virtio.h"

typedef struct {
    VirtIODevice virtio;
    bool read_only;
} VirtIOBlockDevice;

Error initBlockDevice(int id, volatile VirtIODeviceLayout* base, VirtIODevice** output);

#endif
