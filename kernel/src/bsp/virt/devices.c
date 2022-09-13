
#include "devices/devices.h"

#include "bsp/virt/memmap.h"
#include "devices/devfs.h"
#include "devices/serial/uart16550.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "util/text.h"

static Uart16550 serial_mmio;

Error initBoardStdoutDevice() {
    // Initialize the uart device to enable logging
    serial_mmio.base_address = (void*)memory_map[MEM_UART0].base;
    return initUart16550(&serial_mmio);
}

