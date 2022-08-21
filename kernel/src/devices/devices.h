#ifndef _DEVICES_H_
#define _DEVICES_H_

#include "memory/virtptr.h"
#include "error/error.h"

typedef enum {
    DEVICE_BLOCK,
    DEVICE_TTY,
} DeviceType;

typedef int DeviceId;

typedef struct {
    DeviceType type;
    DeviceId id;
} Device;

struct BlockDevice_s;

typedef Error (*BlockDeviceReadFunction)(struct BlockDevice_s* dev, VirtPtr buff, size_t offset, size_t size);
typedef Error (*BlockDeviceWriteFunction)(struct BlockDevice_s* dev, VirtPtr buff, size_t offset, size_t size);

typedef struct {
    BlockDeviceReadFunction read;
    BlockDeviceWriteFunction write;
} BlockDeviceFunctions;

typedef struct BlockDevice_s {
    Device base;
    BlockDeviceFunctions* functions;
    size_t block_size;
    size_t size;
} BlockDevice;

struct TtyDevice_s;

typedef Error (*TtyDeviceReadFunction)(struct TtyDevice_s* dev, VirtPtr buff, size_t size, bool block);
typedef Error (*TtyDeviceWriteFunction)(struct TtyDevice_s* dev, VirtPtr buff, size_t size);
typedef size_t (*TtyDeviceAvailFunction)(struct TtyDevice_s* dev);

typedef struct {
    TtyDeviceReadFunction read;
    TtyDeviceWriteFunction write;
    TtyDeviceAvailFunction avail;
} TtyDeviceFunctions;

typedef struct TtyDevice_s {
    Device base;
    TtyDeviceFunctions* functions;
} TtyDevice;

// Initialize all devices that must be started befor initializing the kernel
Error initBaselineDevices();

// Initialize all devices
Error initDevices();

void registerDevice(Device* device);

Device* getDeviceWithId(DeviceId id);

Device* getDeviceOfType(DeviceType type, DeviceId id);

// Get the device to be used for kernel input/output
TtyDevice* getDefaultTtyDevice();

#endif
