#ifndef _DEVICES_H_
#define _DEVICES_H_

#include "devices/serial/serial.h"

Error initDevices();

Serial getDefaultSerialDevice();

#endif
