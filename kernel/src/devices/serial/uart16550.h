#ifndef _UART16550_H_
#define _UART16550_H_

#include <stdbool.h>
#include <stdint.h>

#include "error/error.h"
#include "task/spinlock.h"
#include "interrupt/plic.h"

typedef struct {
    volatile uint8_t* base_address;
    ExternalInterrupt interrupt;
    SpinLock lock;
    bool initialized;
} Uart16550;

// Initialize the given UART device
Error initUart16550(Uart16550* uart);

// Write a single character to the device
Error writeUart16550(Uart16550* uart, char value);

// Read a single character from the device, return NO_DATA if no data is available
Error readUart16550(Uart16550* uart, char* value);

Error registerUart16550(Uart16550* uart);

Error registerDriverUart16550();

#endif
