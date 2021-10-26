#ifndef _LIBC_ASSERT_H_
#define _LIBC_ASSERT_H_

#include "util/assert.h"

// Simply alias to the util implementation for now
#define assert(COND) ASSERT(COND, #COND)

#define static_assert(COND, ...) STATIC_ASSERT(COND, __VA_ARGS__)

#endif
