#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "task/types.h"

// Enqueue the given process
void enqueueTask(Task* task);

void runNextTask();

void runNextTaskFrom(HartFrame* hart);

Task* pullTaskForHart(HartFrame* hart);

Task* pullTaskFromQueue(ScheduleQueue* queue);

void pushTaskToQueue(ScheduleQueue* queue, Task* task);

Task* removeTaskFromQueue(ScheduleQueue* queue, Task* task);

#endif
