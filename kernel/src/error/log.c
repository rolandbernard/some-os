
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "error/log.h"

#include "devices/devices.h"
#include "devices/serial/tty.h"
#include "error/debuginfo.h"
#include "interrupt/syscall.h"
#include "memory/virtptr.h"
#include "process/types.h"
#include "task/harts.h"
#include "task/syscall.h"
#include "util/text.h"

static SpinLock kernel_log_lock;

static Error logKernelMessageV(const char* fmt, va_list args) {
    // Logging happens to the default serial device
    CharDevice* tty = getDefaultTtyDevice();
    if (tty != NULL) {
        FORMAT_STRINGV(string, fmt, args);
        Error error = writeStringToTty(tty, string);
        return error;
    } else {
        return someError(ENODEV, "There is no default tty device");
    }
}

static void unsafeLogKernelMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logKernelMessageV(fmt, args);
    va_end(args);
}

static void logDebugLocation(uintptr_t addr) {
    HartFrame* hart = getCurrentHartFrame();
    unsafeLogKernelMessage(
        STYLE_DEBUG " ∟ hart %i at \e[m" STYLE_DEBUG_EXT "%p\e[m",
        hart != NULL ? hart->hartid : 0, addr
    );
    const SymbolDebugInfo* symb_info = searchSymbolDebugInfo(addr - 1);
    if (symb_info != NULL) {
        unsafeLogKernelMessage(
            STYLE_DEBUG " (\e[m" STYLE_DEBUG_EXT "%s\e[m" STYLE_DEBUG "+%p)\e[m",
            symb_info->symbol, addr - symb_info->addr
        );
    }
    const LineDebugInfo* line_info = searchLineDebugInfo(addr - 1);
    if (line_info != NULL) {
        unsafeLogKernelMessage(STYLE_DEBUG_LOC " %s:%d", line_info->file, line_info->line);
    }
    unsafeLogKernelMessage("\e[m\n");
}

Error logKernelMessage(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lockSpinLock(&kernel_log_lock);
    Error error = logKernelMessageV(fmt, args);
    unlockSpinLock(&kernel_log_lock);
    va_end(args);
    return error;
}

void logKernelMessageWithDebugLocationAt(uintptr_t pc, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lockSpinLock(&kernel_log_lock);
    logKernelMessageV(fmt, args);
    va_end(args);
    logDebugLocation(pc);
    unlockSpinLock(&kernel_log_lock);
}

void logKernelMessageWithDebugLocation(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    lockSpinLock(&kernel_log_lock);
    logKernelMessageV(fmt, args);
    va_end(args);
    logDebugLocation((uintptr_t)__builtin_return_address(0) - 4);
    unlockSpinLock(&kernel_log_lock);
}

