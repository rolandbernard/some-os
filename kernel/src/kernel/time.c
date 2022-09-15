
#include "kernel/time.h"
#include "devices/devices.h"
#include "error/panic.h"
#include "interrupt/timer.h"

uint32_t getUnixTimeWithFallback() {
    return getNanosecondsWithFallback() / 1000000000UL;
}

Error getUnixTime(uint32_t* time) {
    Time nanos;
    Error error = getNanoseconds(&nanos);
    if (!isError(error)) {
        *time = nanos / 1000000000UL;
    }
    return error;
}

Error setUnixTime(uint32_t time) {
    return setNanoseconds(time * 1000000000UL);
}

#define CLOCK_TO_NANOS (1000000000UL / CLOCKS_PER_SEC)

Time getNanosecondsWithFallback() {
    Time time;
    Error error = getNanoseconds(&time);
    if (isError(error)) {
        time = getTime() * CLOCK_TO_NANOS;
    }
    return time;
}

Error getNanoseconds(Time* nanos) {
    RtcDevice* dev = (RtcDevice*)getDeviceNamed("rtc", 0);
    if (dev != NULL) {
        return dev->functions->get_time(dev, nanos);
    } else {
        return simpleError(ENXIO);
    }
}

Error setNanoseconds(Time nanos) {
    RtcDevice* dev = (RtcDevice*)getDeviceNamed("rtc", 0);
    if (dev != NULL) {
        return dev->functions->set_time(dev, nanos);
    } else {
        return simpleError(ENXIO);
    }
}

