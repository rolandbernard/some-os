
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

uintptr_t printSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    // TODO: REMOVE! This is just for testing. This is really unsafe.
    logKernelMessage((const char*)args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
    return 0;
}

