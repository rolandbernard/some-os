
#include "error/backtrace.h"
#include "error/log.h"
#include "error/panic.h"
#include "interrupt/trap.h"
#include "interrupt/com.h"

noreturn void panicWithBacktrace() {
#ifdef DEBUG
    logBacktrace();
#endif
    panicWithoutBacktrace();
}

noreturn void panicWithoutBacktrace() {
    sendMessageToAll(KERNEL_PANIC, NULL);
    silentPanic();
}

noreturn void silentPanic() {
    for (;;) {
        // Infinite loop after panic
        waitForInterrupt();
    }
}

