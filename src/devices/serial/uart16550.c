
#include "devices/serial/uart16550.h"

Error initUart16550(Uart16550* uart) {
    if (!uart->initialized) {
        // Set the word length to 8-bits by writing 1 into LCR[1:0]
        uart->base_address[3] = (1 << 0) | (1 << 1);
        // Enable FIFO
        uart->base_address[2] = 1 << 0;

        uint16_t divisor = 600;

        // Enable divisor latch
        uint8_t lcr = uart->base_address[3];
        uart->base_address[3] = lcr | 1 << 7;
        // Write divisor
        uart->base_address[0] = divisor & 0xff;
        uart->base_address[1] = divisor >> 8;
        // Close divisor latch
        uart->base_address[3] = lcr;

        uart->initialized = true;
    }
    return SUCCESS;
}

Error writeUart16550(Uart16550* uart, char value) {
    if (uart->initialized) {
        uart->base_address[0] = value;
        return SUCCESS;
    } else {
        return NOT_INITIALIZED;
    }
}

Error readUart16550(Uart16550* uart, char* value) {
    if (uart->initialized) {
        return UNSUPPORTED;
    } else {
        return NOT_INITIALIZED;
    }
}

Serial serialUart16550(Uart16550* uart) {
    Serial ret = {
        .data = uart,
        .write = (SerialWriteFunction)writeUart16550,
        .read = (SerialReadFunction)readUart16550,
    };
    return ret;
}

