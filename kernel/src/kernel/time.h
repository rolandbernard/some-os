#ifndef _TIME_H_
#define _TIME_H_

#include "stdint.h"

#include "error/error.h"

typedef uint64_t Time;

uint32_t getUnixTimeWithFallback();

Error getUnixTime(uint32_t* time);

Error setUnixTime(uint32_t time);

Time getNanosecondsWithFallback();

Error getNanoseconds(Time* nanos);

Error setNanoseconds(Time nanos);

#endif
