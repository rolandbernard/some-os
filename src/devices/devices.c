
#include "devices/devices.h"

#include "devices/serial/uart16550.h"
#include "memory/memmap.h"

static Uart16550 serial_mmio;

Error initBaselineDevices() {
    // Initialize the uart device to enable logging
    serial_mmio.base_address = (void*)memory_map[VIRT_UART0].base;
    CHECKED(initUart16550(&serial_mmio));
    return simpleError(SUCCESS);
}

Error initDevices() {
    return simpleError(SUCCESS);
}

Serial getDefaultSerialDevice() {
    return serialUart16550(&serial_mmio);
}

