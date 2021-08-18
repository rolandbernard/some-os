#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "process/process.h"

// Initialize CSRs for traps
void trapInit();

// Simple wrapper around wfi
void waitForInterrupt();

// Enter process at pc into user mode
void enterUserMode(Process* process, void* pc);

// Enter process at pc into supervisor mode
void enterKernelMode(Process* process, void* pc);

#endif
