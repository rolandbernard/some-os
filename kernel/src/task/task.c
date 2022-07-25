
#include <assert.h>

#include "task/task.h"

#include "memory/virtmem.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "process/signals.h"
#include "task/harts.h"
#include "interrupt/trap.h"
#include "task/schedule.h"
#include "task/types.h"

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, uintptr_t asid, PageTable* table) {
    frame->hart = NULL; // Set to NULL for now. Will be set when enqueuing
    frame->regs[REG_RETURN_ADDRESS] = 0;
    frame->regs[REG_STACK_POINTER] = sp;
    frame->regs[REG_GLOBAL_POINTER] = gp;
    frame->pc = pc;
    frame->satp = satpForMemory(asid, table);
}

Task* createTask() {
    return zalloc(sizeof(Task));
}

Task* createKernelTask(void* enter, size_t stack_size, Priority priority) {
    Task* task = createTask();
    if (task == NULL) {
        return NULL;
    }
    task->stack = kalloc(stack_size);
    initTrapFrame(
        &task->frame, (uintptr_t)task->stack + stack_size, (uintptr_t)getKernelGlobalPointer(),
        (uintptr_t)enter, 0, kernel_page_table
    );
    task->sched.priority = priority;
    task->sched.state = ENQUABLE;
    return task;
}

void deallocTask(Task* task) {
    dealloc(task->stack);
    dealloc(task);
}

void enterTask(Task* task) {
    task->sched.state = RUNNING;
    HartFrame* hart = getCurrentHartFrame();
    task->frame.hart = hart;
    if (hart != NULL) {
        hart->frame.regs[REG_STACK_POINTER] = (uintptr_t)hart->stack_top;
        assert(hart->spinlocks_locked == 0);
    }
    task->times.entered = getTime();
    if (task->process == NULL) {
        enterKernelMode(&task->frame);
    } else {
        if (handlePendingSignals(task)) {
            enterUserMode(&task->frame);
        } else {
            runNextTask();
        }
    }
}

Task* getCurrentTask() {
    TrapFrame* frame = readSscratch();
    if (frame != NULL && frame->hart != NULL) {
        return (Task*)frame;
    } else {
        return NULL;
    }
}

VirtPtr virtPtrForTask(uintptr_t addr, Task* task) {
    if (task->process == NULL) {
        return virtPtrForKernel((void*)addr);
    } else {
        return virtPtrFor(addr, task->process->memory.mem);
    }
}

