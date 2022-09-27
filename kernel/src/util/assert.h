#ifndef _ASSERT_H_
#define _ASSERT_H_

#include "error/log.h"
#include "error/panic.h"
#include "util/macro.h"

#ifdef DEBUG
#define ASSERT(COND, ...) \
    { if (!(COND)) { panic(KERNEL_ERROR("Assertion failed: %s", IFE(__VA_ARGS__)(#COND) __VA_ARGS__)); } }
#else
#define ASSERT(COND, ...) { /* NOOP */ }
#endif

#define STATIC_ASSERT(COND, ...) _Static_assert(COND, IFE(__VA_ARGS__)(#COND) __VA_ARGS__)

#endif
