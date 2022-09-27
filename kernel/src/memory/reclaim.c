
#include "memory/kalloc.h"
#include "task/spinlock.h"

#include "memory/reclaim.h"

typedef struct Reclaimable_s {
    struct Reclaimable_s* next;
    Priority priority;
    ReclaimFunction reclaim;
    void* udata;
} Reclaimable;

static SpinLock reclaimable_lock;
static Reclaimable* reclaimable = NULL;

void registerReclaimable(Priority priority, ReclaimFunction function, void* udata) {
    lockSpinLock(&reclaimable_lock);
    Reclaimable* reclaim = kalloc(sizeof(Reclaimable));
    reclaim->priority = priority;
    reclaim->reclaim = function;
    reclaim->udata = udata;
    reclaim->next = reclaimable;
    reclaimable = reclaim;
    unlockSpinLock(&reclaimable_lock);
}

void unregisterReclaimable(Priority priority, ReclaimFunction function, void* udata) {
    lockSpinLock(&reclaimable_lock);
    Reclaimable** current = &reclaimable;
    while (*current != NULL) {
        Reclaimable* reclaim = *current;
        if (reclaim->priority == priority && reclaim->reclaim == function && reclaim->udata == udata) {
            *current = reclaim->next;
            dealloc(reclaim);
        } else {
            current = &reclaim->next;
        }
    }
    unlockSpinLock(&reclaimable_lock);
}

bool tryReclaimingMemory(Priority priority) {
    lockSpinLock(&reclaimable_lock);
    Reclaimable* current = reclaimable;
    while (current != NULL) {
        if (current->priority >= priority) {
            if (current->reclaim(priority, current->udata)) {
                unlockSpinLock(&reclaimable_lock);
                return true;
            }
        }
        current = current->next;
    }
    unlockSpinLock(&reclaimable_lock);
    return false;
}

