
#include "error/log.h"
#include "interrupt/trap.h"

void panic() {
    logKernelMessage("[!] Kernel panic!");
    for (;;) {
        // Infinite loop after panic
        waitForInterrupt();
    }
}

