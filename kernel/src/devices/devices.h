#ifndef _DEVICES_H_
#define _DEVICES_H_

#include "error/error.h"
#include "kernel/time.h"
#include "memory/virtptr.h"

typedef enum {
    DEVICE_BLOCK,
    DEVICE_CHAR,
    DEVICE_RTC,
} DeviceType;

typedef int DeviceId;

typedef struct {
    DeviceType type;
    DeviceId id;
    const char* name;
    size_t name_id;
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
    const BlockDeviceFunctions* functions;
    size_t block_size;
    size_t size;
} BlockDevice;

struct CharDevice_s;

typedef Error (*CharDeviceReadFunction)(struct CharDevice_s* dev, VirtPtr buff, size_t size, size_t* read, bool block);
typedef Error (*CharDeviceWriteFunction)(struct CharDevice_s* dev, VirtPtr buff, size_t size, size_t* written);
typedef Error (*CharDeviceIoctlFunction)(struct CharDevice_s* dev, size_t request, VirtPtr argp, uintptr_t* res);
typedef bool (*CharDeviceWillBlockFunction)(struct CharDevice_s* dev, bool write);

typedef struct {
    CharDeviceReadFunction read;
    CharDeviceWriteFunction write;
    CharDeviceIoctlFunction ioctl;
    CharDeviceWillBlockFunction is_ready;
} CharDeviceFunctions;

typedef struct CharDevice_s {
    Device base;
    const CharDeviceFunctions* functions;
} CharDevice;

struct RtcDevice_s;

typedef Error (*RtcDeviceGetTimeFunction)(struct RtcDevice_s* dev, Time* time);
typedef Error (*RtcDeviceSetTimeFunction)(struct RtcDevice_s* dev, Time time);

typedef struct {
    RtcDeviceGetTimeFunction get_time;
    RtcDeviceSetTimeFunction set_time;
} RtcDeviceFunctions;

typedef struct RtcDevice_s {
    Device base;
    const RtcDeviceFunctions* functions;
} RtcDevice;

void registerDevice(Device* device);

Device* getDeviceWithId(DeviceId id);

Device* getDeviceNamed(const char* name, size_t name_id);

// This is only for use with the devfs implementation
Device* getNthDevice(size_t nth, bool* fst);

// Get the device to be used for kernel output
CharDevice* getStdoutDevice();

// Initialize the device for kernel stdout
Error initStdoutDevice();

Error initBoardStdoutDevice();

Error initInterruptDevice();

Error initDevices();

#endif
