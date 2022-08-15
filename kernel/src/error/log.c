
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "error/log.h"

#include "process/types.h"
#include "util/text.h"
#include "devices/devices.h"
#include "devices/serial/tty.h"
#include "interrupt/syscall.h"
#include "memory/virtptr.h"
#include "task/syscall.h"

static SpinLock kernel_log_lock;

Error logKernelMessage(const char* fmt, ...) {
    // Logging happens to the default serial device
    TtyDevice* tty = getDefaultTtyDevice();
    FORMAT_STRING(string, fmt);
    lockSpinLock(&kernel_log_lock);
    Error error = writeToTty(tty, "%s", string);
    unlockSpinLock(&kernel_log_lock);
    return error;
}

static void* writeVirtPtrString(VirtPtrBufferPart part, void* udata) {
    TtyDevice* tty = getDefaultTtyDevice();
    writeStringNToTty(tty, part.address, part.length);
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

