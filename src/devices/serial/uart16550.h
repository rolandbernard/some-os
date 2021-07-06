#ifndef _UART16550_H_
#define _UART16550_H_

#include <stdbool.h>
#include <stdint.h>

#include "error/error.h"

typedef struct {
    bool initialized;
    uint8_t* base_address;
} Uart16550;

Error initUart16550(Uart16550* uart);

Error writeUart16550(Uart16550* uart, char value);

Error readUart16550(Uart16550* uart, char* value);

#endif
