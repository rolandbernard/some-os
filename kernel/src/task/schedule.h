#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include <stdnoreturn.h>

#include "task/types.h"

// Enqueue the given process
void enqueueTask(Task* task);

noreturn void runNextTask();

noreturn void runNextTaskFrom(HartFrame* hart);

Task* pullTaskForHart(HartFrame* hart);

Task* pullTaskFromQueue(ScheduleQueue* queue);

void pushTaskToQueue(ScheduleQueue* queue, Task* task);

Task* removeTaskFromQueue(ScheduleQueue* queue, Task* task);

void moveTaskToState(Task* task, TaskState state);

#endif
