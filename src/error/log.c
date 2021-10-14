
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

static SpinLock kernel_log_lock;

Error logKernelMessage(const char* fmt, ...) {
    // Logging happens to the default serial device
    Serial serial = getDefaultSerialDevice();
    FORMAT_STRING(string, fmt);
    lockSpinLock(&kernel_log_lock);
    Error error = writeToSerial(serial, "%s", string);
    unlockSpinLock(&kernel_log_lock);
    return error;
}

static void* writeVirtPtrString(VirtPtrBufferPart part, void* udata) {
    Serial serial = getDefaultSerialDevice();
    writeStringNToSerial(serial, part.address, part.length);
    return udata;
}

void printSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    VirtPtr str;
    if (frame->hart == NULL) {
        str = virtPtrForKernel((void*)args[0]);
    } else {
        Process* process = (Process*)frame;
        str = virtPtrFor(args[0], process->memory.mem);
    }
    size_t length = strlenVirtPtr(str);
    virtPtrPartsDo(str, length, writeVirtPtrString, NULL, false);
}

