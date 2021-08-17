#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include "process/process.h"

void trapInit();

void waitForInterrupt();

void enterUserMode(Process* process, void* pc);

void enterKernelMode(Process* process, void* pc);

#endif
