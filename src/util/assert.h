#ifndef _ASSERT_H_
#define _ASSERT_H_

#include "error/log.h"
#include "error/panic.h"

#define ASSERT(COND) { if (!(COND)) { \
        KERNEL_LOG("Assertion failed: %s", #COND); \
        panic(); \
    } } \

#endif
