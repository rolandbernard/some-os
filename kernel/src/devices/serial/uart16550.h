#ifndef _UART16550_H_
#define _UART16550_H_

#include <stdbool.h>
#include <stdint.h>

#include "error/error.h"
#include "task/spinlock.h"
#include "interrupt/plic.h"

typedef struct {
    volatile uint8_t* base_address;
    uint32_t ref_clock;
    uint32_t reg_shift;
    ExternalInterrupt interrupt;
    SpinLock lock;
} Uart16550;

// Initialize the given UART device
Error initUart16550(Uart16550* uart);

Error registerUart16550(Uart16550* uart);

Error registerDriverUart16550();

#endif
