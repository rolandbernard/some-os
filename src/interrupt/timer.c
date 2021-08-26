
#include <stddef.h>
#include <string.h>

#include "interrupt/timer.h"

#include "memory/kalloc.h"
#include "memory/memmap.h"

#define MIN_TIME (CLOCKS_PER_SEC / 100)

typedef struct {
    Timeout id;
    Time time;
    TimeoutFunction function;
    void* udata;
} TimeoutEntry;

static Timeout next_id = 0;
static TimeoutEntry* timeouts = NULL;
static size_t capacity = 0;
static size_t length = 0;

Timeout setTimeout(Time delay, TimeoutFunction function, void* udata) {
    if (capacity <= length) {
        capacity += 32;
        timeouts = krealloc(timeouts, capacity * sizeof(TimeoutEntry));
    }
    Timeout id = next_id;
    next_id++;
    timeouts[length].id = id;
    timeouts[length].time = getTime() + delay;
    timeouts[length].function = function;
    timeouts[length].udata = udata;
    length++;
    return id;
}

void clearTimeout(Timeout timeout) {
    for (size_t i = 0; i < length; i++) {
        if (timeouts[i].id == timeout) {
            memmove(timeouts + i, timeouts + i + 1, length - i - 1);
            length--;
            return;
        }
    }
}

static void setTimeCmp(Time time) {
    *(volatile Time*)(memory_map[VIRT_CLINT].base + 0x4000) = time;
}

void handleTimerInterrupt() {
    for (;;) {
        Time time = getTime();
        Time min = 0;
        for (size_t i = 0; i < length;) {
            if (timeouts[i].time < time) {
                timeouts[i].function(time, timeouts[i].udata);
                memmove(timeouts + i, timeouts + i + 1, length - i - 1);
                length--;
            } else {
                if (timeouts[i].time < timeouts[min].time) {
                    min = i;
                }
                i++;
            }
        }
        if (length != 0 && timeouts[min].time < time + MIN_TIME) {
            setTimeCmp(timeouts[min].time);
        } else {
            setTimeCmp(time + MIN_TIME);
        }
    }
}

Time getTime() {
    return *(volatile Time*)(memory_map[VIRT_CLINT].base + 0xbff8);
}

