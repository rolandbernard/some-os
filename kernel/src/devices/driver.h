#ifndef _DRIVER_H_
#define _DRIVER_H_

#include "error/error.h"
#include "kernel/devtree.h"

typedef bool (*DriverCompatibilityCheck)(const char* name);

typedef Error (*DriverInitialization)(DeviceTreeNode* device_node);

typedef struct {
    const char* name;
    DriverCompatibilityCheck check;
    DriverInitialization init;
} Driver;

Error registerAllDrivers();

void registerDriver(Driver* driver);

Driver* findDriverForNode(DeviceTreeNode* node);

Error initDriversForStdoutDevice();

Error initDriversForInterruptDevice();

Error initDriversForDeviceTreeNodes();

#endif
