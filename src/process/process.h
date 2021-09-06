#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>
#include <stddef.h>

#include "error/error.h"
#include "process/types.h"

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, HartFrame* hart, uintptr_t asid, PageTable* table);

Process* createKernelProcess(void* start, Priority priority, size_t stack_size);

Process* createUserProcess(uintptr_t sp, uintptr_t gp, uintptr_t pc, Process* parent, Priority priority);

// Free all data connected with the process
void freeProcess(Process* process);

Pid freeKilledChild(Process* parent, uint64_t* status);

// Enter process into the user ot kernel mode depending on process.pid (pid == 0 -> kernel)
void enterProcess(Process* process);

#endif
