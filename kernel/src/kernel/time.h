#ifndef _TIME_H_
#define _TIME_H_

#include "stdint.h"

typedef uint64_t Time;

uint32_t getUnixTime();

Time getNanoseconds();

#endif
