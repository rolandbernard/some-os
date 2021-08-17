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

Error initProcessSystem();

void initProcess(Process* process, void* stack_top, void* globals, void* start);

void enterProcessAsUser(Process* process);

void enterProcessAsKernel(Process* process);

#endif
