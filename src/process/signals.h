#ifndef _SIGNALS_H_
#define _SIGNALS_H_

#include "process/types.h"

void addSignalToProcess(Process* process, Signal signal);

void setupSignalHandlerState(Process* process);

#endif
