#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>

uint64_t umin(uint64_t a, uint64_t b);

uint64_t umax(uint64_t a, uint64_t b);

int64_t smin(int64_t a, int64_t b);

int64_t smax(int64_t a, int64_t b);

// Does nothing
void noop();

uint64_t hashInt64(uint64_t x);

uint32_t hashInt32(uint32_t x);

uint64_t hashString(const char* s);

#endif
