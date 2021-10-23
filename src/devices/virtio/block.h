#ifndef _BLOCK_H_
#define _BLOCK_H_

#include <assert.h>

#include "devices/virtio/virtio.h"
#include "util/spinlock.h"

#define BLOCK_SECTOR_SIZE 512
#define BLOCK_MAX_REQUESTS (1 << 5)

typedef enum {
    VIRTIO_BLOCK_T_IN = 0,
    VIRTIO_BLOCK_T_OUT = 1,
    VIRTIO_BLOCK_T_FLUSH = 4,
    VIRTIO_BLOCK_T_DISCARD = 11,
    VIRTIO_BLOCK_T_WRITE_ZEROS = 13,
} VirtIOBlockTypes;

typedef enum {
    VIRTIO_BLOCK_S_OK = 0,
    VIRTIO_BLOCK_S_IOERR = 1,
    VIRTIO_BLOCK_S_UNSUPP = 2,
    VIRTIO_BLOCK_S_UNKNOWN,
} VirtIOBlockStatus;

typedef enum {
    VIRTIO_BLOCK_F_SIZE_MAX = 1 << 1,
    VIRTIO_BLOCK_F_SEG_MAX = 1 << 2,
    VIRTIO_BLOCK_F_GEOMETRY = 1 << 4,
    VIRTIO_BLOCK_F_RO = 1 << 5,
    VIRTIO_BLOCK_F_BLK_SIZE = 1 << 6,
    VIRTIO_BLOCK_F_FLUSH = 1 << 9,
    VIRTIO_BLOCK_F_TOPOLOGY = 1 << 10,
    VIRTIO_BLOCK_F_CONFIG_WCE = 1 << 11,
    VIRTIO_BLOCK_F_DISCARD = 1 << 13,
    VIRTIO_BLOCK_F_WRITE_ZEROS = 1 << 14,
} VirtIOBlockFeatures;

typedef struct {
    uint16_t cylinders;
    uint8_t heads;
    uint8_t sectors;
} VirtIOBlockGeometry;

typedef struct {
    uint8_t physical_block_exp;
    uint8_t alignment_offset;
    uint16_t min_io_size;
    uint32_t opt_io_size;
} VirtIOBlockTopology;

typedef struct {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;
    VirtIOBlockGeometry geometry;
    uint32_t blk_size;
    VirtIOBlockTopology topology;
    uint8_t writeback;
    uint8_t padding0[3];
    uint32_t max_discard_sector;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t padding1[3];
} VirtIOBlockConfig;

typedef struct {
    uint32_t blk_type;
    uint32_t reserved;
    uint64_t sector;
} VirtIOBlockRequestHeader;

typedef struct {
    VirtPtr data;
} VirtIOBlockRequestData;

typedef struct {
    uint8_t status;
} VirtIOBlockRequestStatus;

typedef struct {
    VirtIODeviceLayout base;
    VirtIOBlockConfig config;
} VirtIOBlockDeviceLayout;

typedef void (*VirtIOBlockCallback)(Error status, void* udata);

typedef struct VirtIOBlockRequest_s {
    VirtIOBlockRequestHeader header;
    VirtIOBlockRequestData data;
    VirtIOBlockRequestStatus status;
    uint16_t head;
    VirtIOBlockCallback callback;
    void* udata;
    struct VirtIOBlockRequest_s* next;
} VirtIOBlockRequest;

typedef struct {
    VirtIODevice virtio;
    SpinLock lock;
    VirtIOBlockRequest* requests;
    bool read_only;
} VirtIOBlockDevice;

Error initVirtIOBlockDevice(int id, volatile VirtIODeviceLayout* base, VirtIODevice** output);

void virtIOBlockDeviceOperation(
    VirtIOBlockDevice* device, VirtPtr buffer, uint32_t offset, uint32_t size, bool write,
    VirtIOBlockCallback callback, void* udata
);

void virtIOBlockFreePendingRequests(VirtIOBlockDevice* device);

#endif
