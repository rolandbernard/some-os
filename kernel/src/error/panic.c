
#include "error/panic.h"
#include "error/log.h"
#include "interrupt/com.h"
#include "interrupt/trap.h"

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

