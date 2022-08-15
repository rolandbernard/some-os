
#include "devices/devices.h"

#include "devices/devfs.h"
#include "devices/serial/uart16550.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "memory/memmap.h"
#include "util/text.h"

static Uart16550 serial_mmio;

Error initBoardBaselineDevices() {
    // Initialize the uart device to enable logging
    serial_mmio.base_address = (void*)memory_map[VIRT_UART0].base;
    CHECKED(initUart16550(&serial_mmio));
    return simpleError(SUCCESS);
}

Error initBoardDevices() {
    return simpleError(SUCCESS);
}

