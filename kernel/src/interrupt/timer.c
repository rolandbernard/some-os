
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "interrupt/timer.h"

#include "error/log.h"
#include "memory/kalloc.h"
#include "memory/memmap.h"
#include "task/spinlock.h"
#include "task/harts.h"

#ifdef DEBUG
// This makes debugging simpler, but is in no way safe.
#define MAX_TIME (CLOCKS_PER_SEC << 10)
#else
#define MAX_TIME (CLOCKS_PER_SEC / 100)
#endif

#define MAX_IDLE_TIME CLOCKS_PER_SEC

typedef struct TimeoutEntry_s {
    struct TimeoutEntry_s* next;
    Timeout id;
    Time time;
    TimeoutFunction function;
    void* udata;
} TimeoutEntry;

static Timeout next_id = 0;
static SpinLock timeout_lock;
static TimeoutEntry* timeouts = NULL;

Timeout setTimeout(Time delay, TimeoutFunction function, void* udata) {
    return setTimeoutTime(getTime() + delay, function, udata);
}

Timeout setTimeoutTime(Time time, TimeoutFunction function, void* udata) {
    lockSpinLock(&timeout_lock);
    Timeout id = next_id;
    next_id++;
    unlockSpinLock(&timeout_lock);
    TimeoutEntry* entry = kalloc(sizeof(TimeoutEntry));
    entry->id = id;
    entry->time = time;
    entry->function = function;
    entry->udata = udata;
    lockSpinLock(&timeout_lock);
    entry->next = timeouts;
    timeouts = entry;
    unlockSpinLock(&timeout_lock);
    return id;
}

void clearTimeout(Timeout timeout) {
    lockSpinLock(&timeout_lock);
    TimeoutEntry** current = &timeouts;
    while (*current != NULL) {
        if ((*current)->id == timeout) {
            TimeoutEntry* to_remove = *current;
            *current = to_remove->next;
            dealloc(to_remove);
        } else {
            current = &(*current)->next;
        }
    }
    unlockSpinLock(&timeout_lock);
}

void setTimeCmp(Time time) {
    *(volatile Time*)(memory_map[MEM_CLINT].base + 0x4000 + 8 * getCurrentHartId()) = time;
}

void handleTimerInterrupt() {
    Time time = getTime();
    TimeoutEntry* to_call = NULL;
    lockSpinLock(&timeout_lock);
    TimeoutEntry** current = &timeouts;
    while (*current != NULL) {
        if ((*current)->time <= time) {
            TimeoutEntry* to_remove = *current;
            *current = to_remove->next;
            to_remove->next = to_call;
            to_call = to_remove;
        } else {
            current = &(*current)->next;
        }
    }
    unlockSpinLock(&timeout_lock);
    while (to_call != NULL) {
        TimeoutEntry* entry = to_call;
        to_call = entry->next;
        if (entry->function != NULL) {
            entry->function(time, entry->udata);
        }
        dealloc(entry);
    }
}

Time setPreemptionTimer(Task* task) {
    Time time = getTime();
    Time next = UINT64_MAX;
    lockSpinLock(&timeout_lock);
    TimeoutEntry* current = timeouts;
    while (current != NULL) {
        if (current->time < next) {
            next = current->time;
        }
        current = current->next;
    }
    unlockSpinLock(&timeout_lock);
    Time max = task->frame.hart->idle_task == task ? MAX_IDLE_TIME : MAX_TIME;
    if (next < time + max) {
        setTimeCmp(next);
    } else {
        setTimeCmp(time + max);
    }
    return time;
}

Time getTime() {
    return *(volatile Time*)(memory_map[MEM_CLINT].base + 0xbff8);
}

