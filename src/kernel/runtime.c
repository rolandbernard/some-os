
#include <stdint.h>

extern void kernelMain();

extern uint8_t* __bss_start;
extern uint8_t* __bss_end;

static void clearBss() {
    for (uint8_t* ptr = __bss_start; ptr < __bss_end; ptr++) {
        ptr = 0;
    }
}

void runtimeInit() {
    clearBss();
    kernelMain();
}

