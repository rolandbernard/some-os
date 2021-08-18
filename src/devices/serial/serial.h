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

Error writeToSerial(Serial serial, const char* fmt, ...);

Error readLineFromSerial(Serial serial, char* string, size_t length, bool echo);

#endif
