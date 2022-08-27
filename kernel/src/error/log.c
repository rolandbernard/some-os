
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "error/log.h"

#include "devices/devices.h"
#include "devices/serial/serial.h"
#include "error/debuginfo.h"
#include "interrupt/syscall.h"
#include "memory/virtptr.h"
#include "process/types.h"
#include "task/syscall.h"
#include "util/text.h"

static SpinLock kernel_log_lock;

static void logDebugLocation(uintptr_t addr) {
    logKernelMessage(STYLE_DEBUG " ∟ at \e[m" STYLE_DEBUG_EXT "%p \e[m", addr);
    SymbolDebugInfo* symb_info = searchSymbolDebugInfo(addr - 1);
    if (symb_info != NULL) {
        logKernelMessage(
            STYLE_DEBUG " (\e[m" STYLE_DEBUG_EXT "%s\e[m" STYLE_DEBUG "+%p)\e[m",
            symb_info->symbol, addr - symb_info->addr
        );
    }
    LineDebugInfo* line_info = searchLineDebugInfo(addr - 1);
    if (line_info != NULL) {
        logKernelMessage(STYLE_DEBUG_LOC " %s:%d", line_info->file, line_info->line);
    }
    logKernelMessage("\e[m\n");
}

Error logKernelMessageV(const char* fmt, va_list args) {
    // Logging happens to the default serial device
    Serial serial = getDefaultSerialDevice();
    FORMAT_STRINGV(string, fmt, args);
    lockSpinLock(&kernel_log_lock);
    Error error = writeToSerial(serial, "%s", string);
    unlockSpinLock(&kernel_log_lock);
    return error;
}

Error logKernelMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Error error = logKernelMessageV(fmt, args);
    va_end(args);
    return error;
}

void logKernelMessageWithDebugLocationAt(uintptr_t pc, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logKernelMessageV(fmt, args);
    va_end(args);
    logDebugLocation(pc);
}

void logKernelMessageWithDebugLocation(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logKernelMessageV(fmt, args);
    va_end(args);
    logDebugLocation((uintptr_t)__builtin_return_address(0) - 4);
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

