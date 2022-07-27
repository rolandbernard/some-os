#ifndef _PANIC_H_
#define _PANIC_H_

#include <stdnoreturn.h>

#include "error/log.h"

// Terminate the kernel
noreturn void panicWithoutBacktrace();

noreturn void panicWithBacktrace();

noreturn void silentPanic();

#define panic() {                       \
    KERNEL_LOG("[!] Kernel panic!")     \
    panicWithBacktrace();               \
}

#endif
