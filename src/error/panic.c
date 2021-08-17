
#include "error/log.h"
#include "interrupt/trap.h"

void panic() {
    logKernelMessage("[!] Kernel panic!");
    for (;;) {
        waitForInterrupt();
    }
}

