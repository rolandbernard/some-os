#ifndef _UART16550_H_
#define _UART16550_H_

#include <stdbool.h>
#include <stdint.h>

#include "error/error.h"
#include "util/spinlock.h"
#include "devices/serial/serial.h"

typedef struct {
    volatile uint8_t* base_address;
    SpinLock lock;
    bool initialized;
} Uart16550;

// Initialize the given UART device
Error initUart16550(Uart16550* uart);

// Write a single character to the device
Error writeUart16550(Uart16550* uart, char value);

// Read a single character from the device, return NO_DATA if no data is available
Error readUart16550(Uart16550* uart, char* value);

// Convert UART device into a Serial interface struct
Serial serialUart16550(Uart16550* uart);

#endif
