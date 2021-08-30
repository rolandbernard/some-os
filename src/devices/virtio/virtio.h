#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <stdint.h>
#include <stddef.h>

#include "error/error.h"

#define MMIO_VIRTIO_COUNT 8
#define MMIO_VIRTIO_MAGIC 0x74726976

typedef enum {
    MAGIC_VALUE = 0x000,
    VERSION = 0x004,
    DEVICE_ID = 0x008,
    VENDOR_ID = 0x00c,
    HOST_FEATURES = 0x010,
    HOST_FEATURES_SEL = 0x014,
    GUEST_FEATURES = 0x020,
    GUEST_FEATURES_SEL = 0x024,
    GUEST_PAGE_SIZE = 0x028,
    QUEUE_SEL = 0x030,
    QUEUE_NUM_SEL = 0x034,
    QUEUE_NUM = 0x038,
    QUEUE_ALIGN = 0x03c,
    QUEUE_PFN = 0x040,
    QUEUE_NOTIFY = 0x050,
    INTERRUPT_STATUS = 0x060,
    INTERRUPT_ACK = 0x064,
    STATUS = 0x070,
    CONFIG = 0x100,
} VirtIOOffsets;

typedef struct {
    uint32_t magic_value;
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t host_features;
    uint32_t host_features_sel;
    uint32_t padding0[2];
    uint32_t guest_features;
    uint32_t guest_features_sel;
    uint64_t guest_page_size;
    uint32_t queue_sel;
    uint32_t queue_num_sel;
    uint32_t queue_num;
    uint32_t queue_align;
    uint32_t queue_pfn;
    uint32_t padding1[3];
    uint32_t queue_notify;
    uint32_t padding2[3];
    uint32_t interrupt_status;
    uint32_t interrupt_ack;
    uint32_t padding3[2];
    uint32_t status;
    uint32_t padding4[35];
    uint32_t config;
} VirtIODeviceLayout;

typedef enum {
    VIRTIO_NONE = 0,
    VIRTIO_NETWORK = 1,
    VIRTIO_BLOCK = 2,
    VIRTIO_CONSOLE = 3,
    VIRTIO_ENTROPY = 4,
    VIRTIO_MEMORY = 5,
    VIRTIO_IO = 6,
    VIRTIO_RPMSG = 7,
    VIRTIO_SCSI = 8,
    VIRTIO_9P = 9,
    VIRTIO_WLAN = 10,
} VirtIODeviceTypes;

Error initVirtIODevices();

VirtIODeviceLayout* getDeviceWithId(int id);

VirtIODeviceLayout* getAnyDeviceOfType(VirtIODeviceTypes type);

size_t getDeviceCountOfType(VirtIODeviceTypes type);

void getDevicesOfType(VirtIODeviceTypes type, VirtIODeviceLayout** devices);

#endif
