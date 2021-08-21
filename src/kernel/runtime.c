
#include <stdint.h>
#include <string.h>

extern void kernelMain();

extern void __bss_start;
extern void __bss_end;

static void clearBss() {
    memset(&__bss_start, 0, &__bss_end - &__bss_start);
}

void runtimeInit() {
    clearBss();
    kernelMain();
}

