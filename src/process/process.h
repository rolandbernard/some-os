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
    void* regs[31];
    void* pc;
    void* stack_top;
    void* globals;
    ProcessState state;
} Process;

// Initialize process system
Error initProcessSystem();

// Initialize a process for the given stack_top, globals and start pc
void initProcess(Process* process, void* stack_top, void* globals, void* start);

// Enter process into user mode
void enterProcessAsUser(Process* process);

// Enter process into supervisor mode
void enterProcessAsKernel(Process* process);

#endif
