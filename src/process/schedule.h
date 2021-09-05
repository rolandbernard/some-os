#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "process/types.h"

// Enqueue the given process
void enqueueProcess(Process* process);

Process* pullProcessFromQueue(ScheduleQueue* queue);

void pushProcessToQueue(ScheduleQueue* queue, Process* process);

Process* removeProccesFromQueue(ScheduleQueue* queue, Process* process);

#endif
