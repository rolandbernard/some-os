#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>

#include "error/error.h"

typedef enum {
    READY,
    RUNNING,
    TERMINATED,
} ProcessState;

typedef struct {
    uintptr_t regs[31];
    uintptr_t pc;
    uintptr_t stack_top;
    uintptr_t globals;
    ProcessState state;
} Process;

// Initialize process system
Error initProcessSystem();

// Initialize a process for the given stack_top, globals and start pc
void initProcess(Process* process, uintptr_t stack_top, uintptr_t globals, uintptr_t start);

// Enter process into user mode
void enterProcessAsUser(Process* process);

// Enter process into supervisor mode
void enterProcessAsKernel(Process* process);

#endif
