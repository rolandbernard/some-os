#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include "process/process.h"

// Create and set a hart frame for the executing hart
HartFrame* setupHartFrame();

// Enqueue the given process
void enqueueProcess(Process* process);

#endif
