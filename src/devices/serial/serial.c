
#include <stdarg.h>
#include <stdio.h>

#include "devices/serial/serial.h"

#include "util/text.h"

Error writeStringToSerial(Serial serial, const char* str) {
    while (*str != 0) {
        CHECKED(serial.write(serial.data, *str));
        str++;
    }
    return simpleError(SUCCESS);
}

Error writeStringNToSerial(Serial serial, const char* str, size_t length) {
    while (length > 0) {
        CHECKED(serial.write(serial.data, *str));
        length--;
        str++;
    }
    return simpleError(SUCCESS);
}

Error writeToSerial(Serial serial, const char* fmt, ...) {
    FORMAT_STRING(string, fmt);
    return writeStringToSerial(serial, string);
}

Error readLineFromSerial(Serial serial, char* string, size_t length, bool echo) {
    while (length > 1) {
        Error status;
        do {
            status = serial.read(serial.data, string);
        } while (status.kind == NO_DATA);
        if (isError(status)) {
            *string = 0;
            return status;
        }
        if (echo) {
            CHECKED(serial.write(serial.data, *string))
        }
        // There is no default \n and \r expantion
        if (*string == '\r') {
            break;
        }
        string++;
        length--;
    }
    if (length > 0) {
        *string = 0;
    }
    return simpleError(SUCCESS);
}

