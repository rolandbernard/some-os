
#include "error/log.h"
#include "error/panic.h"
#include "interrupt/trap.h"

noreturn void panic() {
    KERNEL_LOG("[!] Kernel panic!");
    for (;;) {
        // Infinite loop after panic
        waitForInterrupt();
    }
}

