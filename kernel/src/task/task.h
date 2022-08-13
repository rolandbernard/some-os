#ifndef _TASK_H_
#define _TASK_H_

#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>

#include "error/error.h"
#include "memory/virtptr.h"
#include "task/types.h"
#include "memory/pagetable.h"

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, uintptr_t asid, PageTable* table);

Task* createTask();

Task* createKernelTask(void* enter, size_t stack_size, Priority priority);

void deallocTask(Task* task);

noreturn void enterTask(Task* task);

Task* getCurrentTask();

VirtPtr virtPtrForTask(uintptr_t addr, Task* task);

#endif
