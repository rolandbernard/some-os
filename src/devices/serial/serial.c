
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "devices/serial/serial.h"

#include "util/text.h"

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
    FORMAT_STRING(string, fmt);
    return writeStringToSerial(serial, string);
}

