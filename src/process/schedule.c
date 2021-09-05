
#include "process/schedule.h"

#include "interrupt/trap.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "memory/virtmem.h"
#include "process/process.h"

extern void __global_pointer;
extern void __stack_top;

HartFrame* harts = NULL;
size_t hart_count = 0;

HartFrame* setupHartFrame() {
    TrapFrame* existing = readSscratch();
    if (existing == NULL) {
        size_t id = hart_count;
        hart_count++;
        harts = krealloc(harts, hart_count);
        // Globals and stack_top should be changed for all but the primary hart
        harts[id].globals = &__global_pointer;
        harts[id].stack_top = &__stack_top;
        initTrapFrame(&harts[id].frame, 0, 0, 0, NULL, 0, kernel_page_table);
        writeSscratch(&harts[id].frame);
        return &harts[id];
    } else {
        if (existing->hart == NULL) {
            return (HartFrame*)existing;
        } else {
            return existing->hart;
        }
    }
}

void enqueueProcess(Process* process) {
    // TODO
    if (process->state == READY) {
        enterProcess(process);
    } else {
        for (;;) {
            waitForInterrupt();
        }
    }
}

