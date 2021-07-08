#ifndef _UART16550_H_
#define _UART16550_H_

#include <stdbool.h>
#include <stdint.h>

#include "error/error.h"
#include "devices/serial/serial.h"

typedef struct {
    volatile uint8_t* const base_address;
    bool initialized;
} Uart16550;

Error initUart16550(Uart16550* uart);

Error writeUart16550(Uart16550* uart, char value);

Error readUart16550(Uart16550* uart, char* value);

Serial serialUart16550(Uart16550* uart);

#endif
