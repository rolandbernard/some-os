#ifndef _TIME_H_
#define _TIME_H_

#include "stdint.h"

typedef uint64_t Time;

uint32_t getUnixTime();

void setUnixTime(uint32_t time);

Time getNanoseconds();

void setNanoseconds(Time nanos);

#endif
