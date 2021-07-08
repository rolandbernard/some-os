
#include <stddef.h>

#include "devices/devices.h"
#include "devices/serial/serial.h"

void kernelMain() {
    initDevices();

    // Just some testing code
    Serial serial = getDefaultSerialDevice();
    writeToSerial(serial, "Hello world!\n");
}

