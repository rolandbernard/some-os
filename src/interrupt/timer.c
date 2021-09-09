
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "interrupt/timer.h"

#include "error/log.h"
#include "memory/kalloc.h"
#include "memory/memmap.h"
#include "util/spinlock.h"

#define MIN_TIME (CLOCKS_PER_SEC / 100)

typedef struct {
    Time time;
    TimeoutFunction function;
    void* udata;
} TimeoutEntry;

static SpinLock timeout_lock;
static TimeoutEntry* timeouts = NULL;
static size_t capacity = 0;
static size_t length = 0;

Timeout setTimeout(Time delay, TimeoutFunction function, void* udata) {
    lockSpinLock(&timeout_lock);
    if (capacity <= length) {
        capacity += 32;
        timeouts = krealloc(timeouts, capacity * sizeof(TimeoutEntry));
        assert(timeouts != NULL);
    }
    Timeout id = length;
    timeouts[id].time = getTime() + delay;
    timeouts[id].function = function;
    timeouts[id].udata = udata;
    length++;
    unlockSpinLock(&timeout_lock);
    return id;
}

void clearTimeout(Timeout timeout) {
    lockSpinLock(&timeout_lock);
    if (timeout < length) {
        memmove(timeouts + timeout, timeouts + timeout + 1, (length - timeout - 1) * sizeof(TimeoutEntry));
        length--;
    }
    unlockSpinLock(&timeout_lock);
}

void setTimeCmp(Time time) {
    *(volatile Time*)(memory_map[VIRT_CLINT].base + 0x4000) = time;
}

void initTimerInterrupt() {
    setTimeCmp(getTime() + MIN_TIME);
}

void handleTimerInterrupt() {
    Time time = getTime();
    Time min = 0;
    lockSpinLock(&timeout_lock);
    for (size_t i = 0; i < length;) {
        if (timeouts[i].time < time) {
            timeouts[i].function(time, timeouts[i].udata);
            memmove(timeouts + i, timeouts + i + 1, (length - i - 1) * sizeof(TimeoutEntry));
            length--;
        } else {
            if (timeouts[i].time < timeouts[min].time) {
                min = i;
            }
            i++;
        }
    }
    unlockSpinLock(&timeout_lock);
    if (length != 0 && timeouts[min].time < time + MIN_TIME) {
        setTimeCmp(timeouts[min].time);
    } else {
        setTimeCmp(time + MIN_TIME);
    }
}

Time getTime() {
    return *(volatile Time*)(memory_map[VIRT_CLINT].base + 0xbff8);
}

