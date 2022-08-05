#ifndef _PANIC_H_
#define _PANIC_H_

#include <stdnoreturn.h>

#include "error/log.h"

#ifdef DEBUG
#include "error/backtrace.h"

#define BACKTRACE() logBacktrace();
#else
#define BABACKTRACE()
#endif

// Terminate the kernel
noreturn void notifyPanic();

noreturn void silentPanic();

#define panic() {                       \
    KERNEL_LOG("[!] Kernel panic!")     \
    BACKTRACE();                        \
    notifyPanic();                      \
}

#endif
