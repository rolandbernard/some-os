#ifndef _SIGNALS_H_
#define _SIGNALS_H_

#include "process/types.h"

// Default is zero, so we don't need to initialize
#define SIG_DFL 0
#define SIG_IGN 1

void addSignalToProcess(Process* process, Signal signal);

bool handlePendingSignals(Task* task);

void returnFromSignal(Task* process);

void clearSignals(Process* process);

#endif
