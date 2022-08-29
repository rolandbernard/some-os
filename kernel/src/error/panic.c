
#include "error/panic.h"
#include "error/log.h"
#include "interrupt/com.h"
#include "interrupt/trap.h"

UnsafeLock global_panic_lock;

void notifyPanic() {
    sendMessageToAll(KERNEL_PANIC, NULL);
}

noreturn void silentPanic() {
    for (;;) {
        // Infinite loop after panic
        waitForInterrupt();
    }
}

