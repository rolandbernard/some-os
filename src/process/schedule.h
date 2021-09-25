#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "process/types.h"

// Enqueue the given process
void enqueueProcess(Process* process);

void runNextProcess();

void runNextProcessFrom(HartFrame* hart);

Process* pullProcessForHart(HartFrame* hart);

Process* pullProcessFromQueue(ScheduleQueue* queue);

void pushProcessToQueue(ScheduleQueue* queue, Process* process);

Process* removeProccesFromQueue(ScheduleQueue* queue, Process* process);

bool moveToSchedState(Process* process, ProcessState state);

#endif
