#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stddef.h>
#include <stdbool.h>

#include "devices/devices.h"
#include "devices/serial/ttyctl.h"
#include "error/error.h"
#include "interrupt/plic.h"
#include "task/spinlock.h"
#include "task/task.h"

typedef Error (*UartWriteFunction)(void* uart, char value);
typedef Error (*UartReadFunction)(void* uart, char* value);

typedef struct {
    CharDevice base;
    void* uart_data;
    UartWriteFunction write_func;
    UartReadFunction read_func;
    char* buffer;
    size_t buffer_start;
    size_t buffer_count;
    size_t buffer_capacity;
    size_t line_delim_count;
    Termios ctrl;
    SpinLock lock;
    Task* blocked;
} UartTtyDevice;

UartTtyDevice* createUartTtyDevice(void* uart, UartWriteFunction write, UartReadFunction read);

void uartTtyDataReady(UartTtyDevice* uart_tty);

// Write null-terminated string to the given serial device
Error writeStringToTty(CharDevice* dev, const char* str);

// Write string of given length to the given serial device
Error writeStringNToTty(CharDevice* dev, const char* str, size_t length);

// Write format string to the serial device
Error writeToTty(CharDevice* dev, const char* fmt, ...);

#endif
