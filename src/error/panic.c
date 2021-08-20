
#include "error/log.h"
#include "interrupt/trap.h"

void panic() {
    KERNEL_LOG("[!] Kernel panic!");
    for (;;) {
        // Infinite loop after panic
        waitForInterrupt();
    }
}

