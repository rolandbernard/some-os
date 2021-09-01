
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "error/log.h"

#include "util/text.h"
#include "devices/devices.h"
#include "devices/serial/serial.h"
#include "interrupt/syscall.h"

Error logKernelMessage(const char* fmt, ...) {
    // Logging happens to the default serial device
    Serial serial = getDefaultSerialDevice();
    FORMAT_STRING(string, fmt);
    return writeToSerial(serial, "%s", string);
}

uintptr_t printSyscall(bool is_kernel, Process* process, SyscallArgs args) {
    logKernelMessage("%s", (const char*)args[0]);
    return 0;
}

Error initLogSystem() {
    registerSyscall(0, printSyscall);
    return simpleError(SUCCESS);
}

