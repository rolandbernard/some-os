#ifndef _RTC_GOLDFISH_H_
#define _RTC_GOLDFISH_H_

#include <stdbool.h>
#include <stdint.h>

#include "error/error.h"
#include "task/spinlock.h"
#include "interrupt/plic.h"

typedef struct {
    volatile uint8_t* base_address;
    ExternalInterrupt interrupt;
    SpinLock lock;
} GoldfishRtc;

Error registerDriverGoldfishRtc();

#endif
