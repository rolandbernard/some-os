#ifndef _PANIC_H_
#define _PANIC_H_

#include <stdnoreturn.h>

#include "error/log.h"
#include "task/harts.h"
#include "util/unsafelock.h"

#ifdef DEBUG
#include "error/backtrace.h"

#define BACKTRACE() logBacktrace();
#else
#define BABACKTRACE()
#endif

extern UnsafeLock global_panic_lock;

// Terminate the kernel
noreturn void notifyPanic();

noreturn void silentPanic();

#define panic() {                                                                   \
    if (tryLockingUnsafeLock(&global_panic_lock)) {                                 \
        KERNEL_ERROR("Kernel panic!" STYLE_DEBUG " on hart %u", getCurrentHartId()) \
        BACKTRACE();                                                                \
    }                                                                               \
    notifyPanic();                                                                  \
}

#endif
