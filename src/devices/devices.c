
#include "devices/devices.h"

#include "devices/serial/uart16550.h"

static Uart16550 serial_mmio = {
    .base_address = (void*)0x10000000,
    .initialized = false,
};

Error initBaselineDevices() {
    // Initialize the uart device to enable logging
    CHECKED(initUart16550(&serial_mmio));
    return simpleError(SUCCESS);
}

Error initDevices() {
    return simpleError(SUCCESS);
}

Serial getDefaultSerialDevice() {
    return serialUart16550(&serial_mmio);
}

