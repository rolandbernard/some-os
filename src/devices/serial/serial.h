#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <stddef.h>
#include <stdbool.h>

#include "error/error.h"

typedef Error (*SerialWriteFunction)(void* data, char value);

typedef Error (*SerialReadFunction)(void* data, char* value);

typedef struct {
    void* data;
    SerialWriteFunction write;
    SerialReadFunction read;
} Serial;

// Write null-terminated string to the given serial device
Error writeStringToSerial(Serial serial, const char* str);

// Write format string to the serial device
Error writeToSerial(Serial serial, const char* fmt, ...);

// Read a line from the serial device
Error readLineFromSerial(Serial serial, char* string, size_t length, bool echo);

#endif
