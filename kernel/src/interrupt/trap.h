#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdnoreturn.h>

#include "task/types.h"

// Initialize CSRs for traps
void initTraps();

// Write to the supervisor interrupt enable register
void enableInterrupts();

// Simple wrapper around wfi
void waitForInterrupt();

// Enter process at pc into user mode
noreturn void enterUserMode(TrapFrame* process);

// Enter process at pc into supervisor mode
noreturn void enterKernelMode(TrapFrame* process);

// Enter frame in machine mode. Used for nested traps.
noreturn void enterKernelModeTrap(TrapFrame* process);

#endif
