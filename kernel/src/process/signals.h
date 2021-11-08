#ifndef _SIGNALS_H_
#define _SIGNALS_H_

#include "process/types.h"

// Default is zero, so we don't need to initialize
#define SIG_DFL (void*)0
#define SIG_IGN (void*)1

void addSignalToProcess(Process* process, Signal signal);

void handlePendingSignals(Process* process);

void returnFromSignal(Process* process);

#endif