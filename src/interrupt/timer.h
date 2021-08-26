#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

#include "error/error.h"

#define CLOCKS_PER_SEC 10000000 // 10Mhz

typedef uint64_t Timeout;
typedef uint64_t Time;
typedef void (*TimeoutFunction)(Time time, void* udata);

void handleTimerInterrupt();

Time getTime();

Timeout setTimeout(Time delay, TimeoutFunction function, void* udata);

void clearTimeout(Timeout timeout);

#endif
