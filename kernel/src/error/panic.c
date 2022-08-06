
#include "error/panic.h"
#include "error/log.h"
#include "interrupt/com.h"
#include "interrupt/trap.h"

UnsafeLock global_panic_lock;

noreturn void notifyPanic() {
    sendMessageToAll(KERNEL_PANIC, NULL);
    silentPanic();
}

noreturn void silentPanic() {
    for (;;) {
        // Infinite loop after panic
        waitForInterrupt();
    }
}

