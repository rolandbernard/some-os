#ifndef _DRIVER_H_
#define _DRIVER_H_

#include "error/error.h"
#include "kernel/devtree.h"

typedef bool (*DriverCompatibilityCheck)(const char* name);

typedef Error (*DriverInitialization)(DeviceTreeNode* device_node);

typedef enum {
    DRIVER_FLAGS_NONE = 0,
    DRIVER_FLAGS_INTERRUPT = (1 << 0), // These are initialized earlier
} DriverFlags;

typedef struct Driver_s {
    const char* name;
    DriverFlags flags;
    DriverCompatibilityCheck check;
    DriverInitialization init;
} Driver;

Error registerAllDrivers();

void registerDriver(Driver* driver);

Error initDriversForStdoutDevice();

Error initDriversForInterruptDevices();

Error initDriversForDeviceTreeNodes();

#endif
