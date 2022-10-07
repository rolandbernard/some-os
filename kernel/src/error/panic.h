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
#define BACKTRACE()
#endif

extern UnsafeLock global_panic_lock;

// signal panic to all other harts
void notifyPanic();

noreturn void silentPanic();

// Only here for setting breakpoints on
void panicBreak();

// This is here so we can call if from assembly
void doPanic();

#define panic(...) {                                                                \
    panicBreak();                                                                   \
    if (tryLockingUnsafeLock(&global_panic_lock)) {                                 \
        notifyPanic();                                                              \
        __VA_OPT__(KERNEL_ERROR(__VA_ARGS__));                                      \
        KERNEL_ERROR("Kernel panic!" STYLE_DEBUG " on hart %u", getCurrentHartId()) \
        BACKTRACE();                                                                \
    }                                                                               \
    silentPanic();                                                                  \
}

#endif
