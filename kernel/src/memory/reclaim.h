#ifndef _RECLAIM_H_
#define _RECLAIM_H_

#include <stddef.h>

#include "task/types.h"

typedef bool (*ReclaimFunction)(Priority priority, void* udata);

void registerReclaimable(Priority priority, ReclaimFunction function, void* udata);

void unregisterReclaimable(Priority priority, ReclaimFunction function, void* udata);

bool tryReclaimingMemory(Priority priority);

#endif
