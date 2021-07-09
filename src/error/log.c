
#include "error/log.h"

#include "devices/devices.h"
#include "devices/serial/serial.h"

Error logKernelMessage(const char* msg) {
    Serial serial = getDefaultSerialDevice();
    return writeToSerial(serial, "%s\n", msg);
}

