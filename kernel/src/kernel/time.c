
#include "kernel/time.h"
#include "interrupt/timer.h"
#include "error/panic.h"

uint32_t getUnixTime() {
    return getNanoseconds() / 1000000000UL;
}

void setUnixTime(uint32_t time) {
    setNanoseconds(time * 1000000000UL);
}

#define CLOCK_TO_NANOS (1000000000UL / CLOCKS_PER_SEC)

Time getNanoseconds() {
    // TODO: get the actual time
    return getTime() * CLOCK_TO_NANOS;
}

void setNanoseconds(Time nanos) {
    // TODO: set actual time
    panic();
}

