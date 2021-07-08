
#include "devices/serial/serial.h"

Error writeToSerial(Serial serial, const char* str) {
    while (*str != 0) {
        Error res = serial.write(serial.data, *str);
        if (res != SUCCESS) {
            return res;
        }
        str++;
    }
    return SUCCESS;
}

