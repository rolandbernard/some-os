
#include "kernel/time.h"
#include "interrupt/timer.h"

uint32_t getUnixTime() {
    return getNanoseconds() / 1000000000UL;
}

#define CLOCK_TO_NANOS (1000000000UL / CLOCKS_PER_SEC)

uint64_t getNanoseconds() {
    // TODO: get the actual time
    return getTime() * CLOCK_TO_NANOS;
}

