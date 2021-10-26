#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "process/process.h"

// Initialize CSRs for traps
void initTraps();

// Write to the supervisor interrupt enable register
void enableInterrupts();

// Simple wrapper around wfi
void waitForInterrupt();

// Enter process at pc into user mode
void enterUserMode(TrapFrame* process);

// Enter process at pc into supervisor mode
void enterKernelMode(TrapFrame* process);

#endif
