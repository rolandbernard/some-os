
#include <assert.h>

#include "task/task.h"

#include "interrupt/trap.h"
#include "memory/kalloc.h"
#include "memory/virtmem.h"
#include "memory/virtptr.h"
#include "process/process.h"
#include "process/syscall.h"
#include "task/harts.h"
#include "task/schedule.h"
#include "task/types.h"

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, uintptr_t asid, PageTable* table) {
    frame->hart = NULL; // Set to NULL for now. Will be set when enqueuing
    // frame->regs[REG_RETURN_ADDRESS] = 0;
    frame->regs[REG_STACK_POINTER] = sp;
    frame->regs[REG_GLOBAL_POINTER] = gp;
    frame->pc = pc;
    frame->satp = satpForMemory(asid, table);
}

void initKernelTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t pc) {
    initTrapFrame(frame, sp, (uintptr_t)getKernelGlobalPointer(), pc, 0, kernel_page_table);
    frame->regs[REG_RETURN_ADDRESS] = (uintptr_t)__builtin_return_address(0);
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
    task->stack_top = (uintptr_t)task->stack + stack_size;
    initKernelTrapFrame(&task->frame, task->stack_top, (uintptr_t)enter);
    task->sched.priority = priority;
    moveTaskToState(task, ENQUABLE);
    return task;
}

void deallocTask(Task* task) {
    if (task->process != NULL) {
        removeProcessTask(task);
    }
    dealloc(task->stack);
    dealloc(task);
}

noreturn void enterTask(Task* task) {
    assert(getCurrentTask() == NULL);
    moveTaskToState(task, RUNNING);
    HartFrame* hart = getCurrentHartFrame();
    assert(hart != NULL);
    hart->frame.regs[REG_STACK_POINTER] = (uintptr_t)hart->stack_top;
    assert(hart->spinlocks_locked == 0);
    task->frame.hart = hart;
    task->times.entered = getTime();
    if (task->process == NULL) {
        enterKernelMode(&task->frame);
    } else {
        enterUserMode(&task->frame);
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

