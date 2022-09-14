#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

#include "error/error.h"
#include "task/types.h"
#include "interrupt/clint.h"

#define CLOCKS_PER_SEC 10000000UL // 10Mhz

typedef uint64_t Timeout;
typedef void (*TimeoutFunction)(Time time, void* udata);

void handleTimerInterrupt();

Time setPreemptionTimer(Task* task);

Timeout setTimeout(Time delay, TimeoutFunction function, void* udata);

Timeout setTimeoutTime(Time time, TimeoutFunction function, void* udata);

void clearTimeout(Timeout timeout);

#endif
