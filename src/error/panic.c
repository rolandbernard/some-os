
#include "error/log.h"
#include "error/panic.h"
#include "interrupt/trap.h"
#include "interrupt/com.h"

noreturn void panic() {
    KERNEL_LOG("[!] Kernel panic!");
    sendMessageToAll(KERNEL_PANIC, NULL);
    sendMessageToSelf(KERNEL_PANIC, NULL);
    silentPanic();
}

noreturn void silentPanic() {
    for (;;) {
        // Infinite loop after panic
        waitForInterrupt();
    }
}

