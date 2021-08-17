
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "error/log.h"

#include "util/text.h"
#include "devices/devices.h"
#include "devices/serial/serial.h"
#include "interrupt/syscall.h"

Error logKernelMessage(const char* fmt, ...) {
    Serial serial = getDefaultSerialDevice();
    FORMAT_STRING(string, fmt);
    return writeToSerial(serial, "%s\n", string);
}

void* printSyscall(Process* process, SyscallArgs args) {
    logKernelMessage("%s", args[0]);
    return NULL;
}

Error initLogSystem() {
    registerSyscall(0, printSyscall);
    return simpleError(SUCCESS);
}

