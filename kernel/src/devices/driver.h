#ifndef _DRIVER_H_
#define _DRIVER_H_

#include "error/error.h"
#include "kernel/devtree.h"

typedef bool (*DriverCompatibilityCheck)(const char* name);

typedef Error (*DriverInitialization)(DeviceTreeNode* device_node);

typedef enum {
    DRIVER_FLAGS_NONE = 0,
    DRIVER_FLAGS_INTERRUPT = (1 << 0), // These are initialized earlier
    DRIVER_FLAGS_MMIO = (1 << 1), // These must have there memory regions mapped first
} DriverFlags;

typedef struct Driver_s {
    const char* name;
    DriverFlags flags;
    DriverCompatibilityCheck check;
    DriverInitialization init;
} Driver;

Error registerAllDrivers();

void registerDriver(Driver* driver);

Driver* findDriverForNode(DeviceTreeNode* node);

Error initDriversForStdoutDevice();

Error initDriversForInterruptDevices();

Error initDriversForDeviceTreeNodes();

#endif
