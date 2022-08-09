
#include "devices/serial/uart16550.h"

#include "error/log.h"
#include "task/spinlock.h"

Error initUart16550(Uart16550* uart) {
    lockSpinLock(&uart->lock);
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

        unlockSpinLock(&uart->lock);
        KERNEL_SUBSUCCESS("Initialized UART device");
    } else {
        unlockSpinLock(&uart->lock);
    }
    return simpleError(SUCCESS);
}

Error writeUart16550(Uart16550* uart, char value) {
    lockSpinLock(&uart->lock);
    if (uart->initialized) {
        while (((uart->base_address[5] >> 5) & 0x1) == 0) {
            /* Wait for THR to be empty */
        }
        // Write directly to MMIO
        uart->base_address[0] = value;
        unlockSpinLock(&uart->lock);
        return simpleError(SUCCESS);
    } else {
        unlockSpinLock(&uart->lock);
        return someError(EIO, "Uart16550 is not initialized");
    }
}

Error readUart16550(Uart16550* uart, char* value) {
    lockSpinLock(&uart->lock);
    if (uart->initialized) {
        if ((uart->base_address[5] & 0x1) == 0) {
            // Data Ready == 0 => No data is available
            unlockSpinLock(&uart->lock);
            return simpleError(EAGAIN);
        } else {
            // Data is available
            *value = uart->base_address[0];
            unlockSpinLock(&uart->lock);
            return simpleError(SUCCESS);
        }
    } else {
        unlockSpinLock(&uart->lock);
        return someError(EIO, "Uart16550 is not initialized");
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

