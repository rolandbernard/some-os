
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "error/log.h"

#include "devices/devices.h"
#include "devices/serial/tty.h"
#include "error/debuginfo.h"
#include "interrupt/syscall.h"
#include "memory/kalloc.h"
#include "process/types.h"
#include "task/harts.h"
#include "task/syscall.h"
#include "util/text.h"

typedef struct CachedLog_s {
    struct CachedLog_s* next;
    char data[];
} CachedLog;

CachedLog* cached_logs;
CachedLog* cached_logs_tail;

static void flushCachedLogs() {
    CharDevice* tty = getStdoutDevice();
    if (tty != NULL) {
        while (cached_logs != NULL) {
            CachedLog* log = cached_logs;
            writeStringToTty(tty, log->data);
            dealloc(log);
            cached_logs = log->next;
            if (cached_logs == NULL) {
                cached_logs_tail = NULL;
            }
        }
    }
}

void panicFlushLogs() {
    initStdoutDevice();
    flushCachedLogs();
}

static Error logKernelMessageV(const char* fmt, va_list args) {
    // Logging happens to the default stdout device
    FORMAT_STRINGV(string, fmt, args);
    CharDevice* tty = getStdoutDevice();
    if (tty != NULL) {
        flushCachedLogs();
        Error error = writeStringToTty(tty, string);
        return error;
    } else {
        CachedLog* log = kalloc(sizeof(CachedLog) + strlen(string) + 1);
        memcpy(log->data, string, strlen(string) + 1);
        log->next = NULL;
        if (cached_logs == NULL) {
            cached_logs = log;
        } else {
            cached_logs_tail->next = log;
        }
        cached_logs_tail = log;
        return someError(SUCCESS, "Cached in memory");
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

static SpinLock kernel_log_lock;

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

