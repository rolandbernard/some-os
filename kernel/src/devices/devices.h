#ifndef _DEVICES_H_
#define _DEVICES_H_

#include "devices/serial/serial.h"

// Initialize all devices that must be started befor initializing the kernel
Error initBaselineDevices();

// Initialize all devices
Error initDevices();

// Get the serial device to be used for default input/output
Serial getDefaultSerialDevice();

Error mountDeviceFiles();

#endif
