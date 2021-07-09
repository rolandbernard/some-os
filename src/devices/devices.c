
#include "devices/devices.h"

#include "devices/serial/uart16550.h"

static Uart16550 serial_mmio = {
    .base_address = (void*)0x10000000,
    .initialized = false,
};

Error initBaselineDevices() {
    return initUart16550(&serial_mmio);
}

Error initDevices() {
    return SUCCESS;
}

Serial getDefaultSerialDevice() {
    return serialUart16550(&serial_mmio);
}

