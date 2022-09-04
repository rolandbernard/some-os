
#include "interrupt/plic.h"

ExternalInterrupt getVirtIoInterruptId(int dev) {
    return dev + 1;
}

ExternalInterrupt getUartInterruptId() {
    return 10;
}

