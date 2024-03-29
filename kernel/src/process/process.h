#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>
#include <stddef.h>

#include "error/error.h"
#include "process/types.h"
#include "task/types.h"
#include "memory/memspace.h"

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, uintptr_t asid, MemorySpace* table);

Process* createUserProcess(Process* parent);

Task* createTaskInProcess(Process* process, uintptr_t sp, uintptr_t gp, uintptr_t pc, Priority priority);

void addTaskToProcess(Process* process, Task* task);

void terminateAllProcessTasks(Process* process);

void terminateAllProcessTasksBut(Process* process, Task* keep);

void executeProcessWait(Task* task);

void removeProcessTask(Task* task);

bool shouldTaskWakeup(Task* task);

void deallocProcess(Process* process);

void exitProcess(Process* process, Signal signal, int exit);

void stopProcess(Process* process, Signal signal);

void continueProcess(Process* process, Signal signal);

typedef int (*ProcessFindCallback)(Process* process, void* udata);

int doForProcessWithPid(Pid pid, ProcessFindCallback callback, void* udata);

void doForAllProcess(ProcessFindCallback callback, void* udata);

void signalProcessGroup(Pid pgid, Signal signal);

#endif
