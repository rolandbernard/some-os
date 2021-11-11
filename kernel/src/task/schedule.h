#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "task/types.h"

// Enqueue the given process
void enqueueTask(Task* process);

void runNextTask();

void runNextTaskFrom(HartFrame* hart);

Task* pullTaskForHart(HartFrame* hart);

Task* pullTaskFromQueue(ScheduleQueue* queue);

void pushTaskToQueue(ScheduleQueue* queue, Task* process);

Task* removeTaskFromQueue(ScheduleQueue* queue, Task* process);

#endif
