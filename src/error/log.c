
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "error/log.h"

#include "process/types.h"
#include "util/text.h"
#include "devices/devices.h"
#include "devices/serial/serial.h"
#include "interrupt/syscall.h"
#include "memory/virtptr.h"

Error logKernelMessage(const char* fmt, ...) {
    // Logging happens to the default serial device
    Serial serial = getDefaultSerialDevice();
    FORMAT_STRING(string, fmt);
    return writeToSerial(serial, "%s", string);
}

static void* writeVirtPtrString(VirtPtrBufferPart part, void* udata) {
    Serial serial = getDefaultSerialDevice();
    writeStringNToSerial(serial, part.address, part.length);
    return udata;
}

uintptr_t printSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    VirtPtr str;
    if (frame->hart == NULL) {
        str = virtPtrForKernel((void*)args[0]);
    } else {
        Process* process = (Process*)frame;
        str = virtPtrFor(args[0], process->table);
    }
    size_t length = strlenVirtPtr(str);
    virtPtrPartsDo(str, length, writeVirtPtrString, NULL);
    return 0;
}

