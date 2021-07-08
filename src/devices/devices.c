
#include "devices/devices.h"

#include "devices/serial/uart16550.h"

static Uart16550 serial_mmio = {
    .base_address = (void*)0x10000000,
    .initialized = false,
};

Error initDevices() {
    initUart16550(&serial_mmio);
    return SUCCESS;
}

Serial getDefaultSerialDevice() {
    return serialUart16550(&serial_mmio);
}

