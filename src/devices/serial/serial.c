
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "devices/serial/serial.h"

static Error writeStringToSerial(Serial serial, const char* str) {
    while (*str != 0) {
        Error res = serial.write(serial.data, *str);
        if (res != SUCCESS) {
            return res;
        }
        str++;
    }
    return SUCCESS;
}

Error writeToSerial(Serial serial, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    size_t size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    va_start(args, fmt);
    char string[size + 1];
    vsnprintf(string, size + 1, fmt, args);
    va_end(args);
    return writeStringToSerial(serial, string);
}

