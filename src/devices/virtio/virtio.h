#ifndef _VIRTIO_H_
#define _VIRTIO_H_

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "error/error.h"

#include "memory/pagealloc.h"
#include "memory/virtptr.h"

#define VIRTIO_DEVICE_COUNT 8
#define VIRTIO_MAGIC_NUMBER 0x74726976
#define VIRTIO_MEM_STROBE 0x1000
#define VIRTIO_RING_SIZE (1 << 7)

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
    uint32_t guest_page_size;
    uint32_t padding1[1];
    uint32_t queue_sel;
    uint32_t queue_num_max;
    uint32_t queue_num;
    uint32_t queue_align;
    uint64_t queue_pfn;
    uint32_t padding2[2];
    uint32_t queue_notify;
    uint32_t padding3[3];
    uint32_t interrupt_status;
    uint32_t interrupt_ack;
    uint32_t padding4[2];
    uint32_t status;
    uint32_t padding5[35];
} VirtIODeviceLayout;

typedef enum {
    VIRTIO_NONE = 0,
    VIRTIO_NETWORK = 1,
    VIRTIO_BLOCK = 2,
    VIRTIO_CONSOLE = 3,
    VIRTIO_ENTROPY = 4,
    VIRTIO_MEMORY_BALLOONING = 5,
    VIRTIO_IO_MEMORY = 6,
    VIRTIO_RPMSG = 7,
    VIRTIO_SCSI_HOST = 8,
    VIRTIO_9P_TRANSPORT = 9,
    VIRTIO_WLAN = 10,
    VIRTIO_GPU = 16,
    VIRTIO_INPUT = 18,
    VIRTIO_MEMORY = 24,
    VIRTIO_DEVICE_TYPE_END,
} VirtIODeviceType;

typedef enum {
    VIRTIO_ACKNOWLEDGE = 1,
    VIRTIO_DRIVER = 2,
    VIRTIO_DRIVER_OK = 4,
    VIRTIO_FEATURES_OK = 8,
    VIRTIO_NEEDS_RESET = 64,
    VIRTIO_FAILED = 128,
} VirtIOStatusField;

typedef enum {
    VIRTIO_DESC_NEXT = 1,
    VIRTIO_DESC_WRITE = 2,
    VIRTIO_DESC_INDIRECT = 4,
} VirtIODescriptorFlags;

typedef enum {
    VIRTIO_AVAIL_NO_INTERRUPT = 1,
} VirtIOAvailableFlags;

typedef enum {
    VIRTIO_USED_NO_NOTIFY = 1,
} VirtIOUsedFlags;

typedef struct {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} VirtIODescriptor;

typedef struct {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTIO_RING_SIZE];
    uint16_t event;
} VirtIOAvailable;

typedef struct {
    uint32_t id;
    uint32_t length;
} VirtIOUsedElement;

typedef struct {
    uint16_t flags;
    uint16_t index;
    VirtIOUsedElement ring[VIRTIO_RING_SIZE];
    uint16_t event;
} VirtIOUsed;

typedef struct {
    VirtIODescriptor descriptors[VIRTIO_RING_SIZE];
    VirtIOAvailable available;
    uint8_t padding[PAGE_SIZE - sizeof(VirtIODescriptor[VIRTIO_RING_SIZE]) - sizeof(VirtIOAvailable)];
    VirtIOUsed used;
} VirtIOQueue;

typedef struct {
    volatile VirtIODeviceLayout* mmio;
    volatile VirtIOQueue* queue;
    VirtIODeviceType type;
    uint16_t index;
    uint16_t ack_index;
} VirtIODevice;

Error initVirtIODevices();

VirtIODevice* getDeviceWithId(int id);

VirtIODevice* getAnyDeviceOfType(VirtIODeviceType type);

size_t getDeviceCountOfType(VirtIODeviceType type);

void getDevicesOfType(VirtIODeviceType type, VirtIODevice** devices);

Error setupVirtIOQueue(VirtIODevice* device);

uint16_t fillNextDescriptor(VirtIODevice* device, VirtIODescriptor descriptor);

uint16_t addDescriptorsFor(VirtIODevice* device, VirtPtr buffer, size_t length, VirtIODescriptorFlags flags, bool write);

void sendRequestAt(VirtIODevice* device, uint16_t descriptor);

#endif
