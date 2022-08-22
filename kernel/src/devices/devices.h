#ifndef _DEVICES_H_
#define _DEVICES_H_

#include "memory/virtptr.h"
#include "error/error.h"

typedef enum {
    DEVICE_BLOCK,
    DEVICE_CHAR,
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

struct CharDevice_s;

typedef Error (*CharDeviceReadFunction)(struct CharDevice_s* dev, VirtPtr buff, size_t size, bool block);
typedef Error (*CharDeviceWriteFunction)(struct CharDevice_s* dev, VirtPtr buff, size_t size);
typedef size_t (*CharDeviceAvailFunction)(struct CharDevice_s* dev);

typedef struct {
    CharDeviceReadFunction read;
    CharDeviceWriteFunction write;
    CharDeviceAvailFunction avail;
} CharDeviceFunctions;

typedef struct CharDevice_s {
    Device base;
    CharDeviceFunctions* functions;
} CharDevice;

// Initialize all devices that must be started befor initializing the kernel
Error initBaselineDevices();

// Initialize all devices
Error initDevices();

void registerDevice(Device* device);

Device* getDeviceWithId(DeviceId id);

Device* getDeviceOfType(DeviceType type, DeviceId id);

// Get the device to be used for kernel input/output
CharDevice* getDefaultTtyDevice();

#endif
