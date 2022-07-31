
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
#include "task/syscall.h"

static SpinLock kernel_log_lock;

Error logKernelMessage(const char* fmt, ...) {
    // Logging happens to the default serial device
    Serial serial = getDefaultSerialDevice();
    FORMAT_STRING(string, fmt);
    TrapFrame* lock = criticalEnter();
    lockSpinLock(&kernel_log_lock);
    Error error = writeToSerial(serial, "%s", string);
    unlockSpinLock(&kernel_log_lock);
    criticalReturn(lock);
    return error;
}

static void* writeVirtPtrString(VirtPtrBufferPart part, void* udata) {
    Serial serial = getDefaultSerialDevice();
    writeStringNToSerial(serial, part.address, part.length);
    return udata;
}

SyscallReturn printSyscall(TrapFrame* frame) {
    VirtPtr str;
    if (frame->hart == NULL) {
        str = virtPtrForKernel((void*)SYSCALL_ARG(0));
    } else {
        Task* task = (Task*)frame;
        str = virtPtrForTask(SYSCALL_ARG(0), task);
    }
    size_t length = strlenVirtPtr(str);
    virtPtrPartsDo(str, length, writeVirtPtrString, NULL, false);
    return CONTINUE;
}

