
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "interrupt/timer.h"

#include "error/log.h"
#include "memory/kalloc.h"
#include "memory/memmap.h"
#include "util/spinlock.h"
#include "process/harts.h"

#define MIN_TIME (CLOCKS_PER_SEC / 100)

typedef struct {
    Timeout id;
    Time time;
    TimeoutFunction function;
    void* udata;
} TimeoutEntry;

static Timeout next_id = 0;
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
    Timeout id = next_id;
    next_id++;
    size_t index = length;
    length++;
    timeouts[index].id = id;
    timeouts[index].time = getTime() + delay;
    timeouts[index].function = function;
    timeouts[index].udata = udata;
    unlockSpinLock(&timeout_lock);
    return id;
}

void clearTimeout(Timeout timeout) {
    lockSpinLock(&timeout_lock);
    for (size_t i = 0; i < length;) {
        if (timeouts[i].id == timeout) {
            memmove(timeouts + timeout, timeouts + timeout + 1, (length - timeout - 1) * sizeof(TimeoutEntry));
            length--;
        } else {
            i++;
        }
    }
    unlockSpinLock(&timeout_lock);
}

void setTimeCmp(Time time) {
    *(volatile Time*)(memory_map[VIRT_CLINT].base + 0x4000 + 8 * getCurrentHartId()) = time;
}

void initTimerInterrupt() {
    setTimeCmp(getTime() + MIN_TIME);
}

void handleTimerInterrupt() {
    Time time = getTime();
    Time min = 0;
    size_t timeout_count = 0;
    lockSpinLock(&timeout_lock);
    for (size_t i = 0; i < length; i++) {
        if (timeouts[i].time < time) {
            timeout_count++;
        }
    }
    TimeoutEntry timeouts_to_run[timeout_count];
    timeout_count = 0;
    for (size_t i = 0; i < length;) {
        if (timeouts[i].time < time) {
            timeouts_to_run[timeout_count] = timeouts[i];
            timeout_count++;
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
    for (size_t i = 0; i < timeout_count; i++) {
        timeouts_to_run[i].function(time, timeouts[i].udata);
    }
    if (length != 0 && timeouts[min].time < time + MIN_TIME) {
        setTimeCmp(timeouts[min].time);
    } else {
        setTimeCmp(time + MIN_TIME);
    }
}

Time getTime() {
    return *(volatile Time*)(memory_map[VIRT_CLINT].base + 0xbff8);
}

