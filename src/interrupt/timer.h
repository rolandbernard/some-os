#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

#include "error/error.h"

#define CLOCKS_PER_SEC 10000000 // 10Mhz

typedef void (*TimeoutFunction)(uint64_t time, void* udata);
typedef uint64_t Timeout;

void handleTimerInterrupt();

uint64_t getTime();

Timeout setTimeout(uint64_t delay, TimeoutFunction function, void* udata);

void clearTimeout(Timeout timeout);

#endif
