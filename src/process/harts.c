
#include <stddef.h>
#include <string.h>

#include "process/harts.h"
#include "process/process.h"
#include "memory/kalloc.h"
#include "memory/virtmem.h"

extern void __global_pointer;
extern void __stack_top;

HartFrame* harts = NULL;
size_t hart_count = 0;

HartFrame* setupHartFrame() {
    TrapFrame* existing = readSscratch();
    if (existing == NULL) {
        size_t id = hart_count;
        hart_count++;
        harts = krealloc(harts, hart_count * sizeof(HartFrame));
        memset(&harts[id], 0, sizeof(HartFrame));
        // Globals and stack_top should be changed for all but the primary hart
        harts[id].stack_top = &__stack_top;
        initTrapFrame(&harts[id].frame, (uintptr_t)&__stack_top, (uintptr_t)&__global_pointer, 0, NULL, 0, kernel_page_table);
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

HartFrame* getCurrentHartFrame() {
    TrapFrame* frame = readSscratch();
    if (frame != NULL && frame->hart != NULL) {
        return frame->hart;
    } else {
        return (HartFrame*)frame;
    }
}

void* getKernelGlobalPointer() {
    return &__global_pointer;
}

Process* getCurrentProcess() {
    TrapFrame* frame = readSscratch();
    if (frame != NULL && frame->hart != NULL) {
        return (Process*)frame;
    } else {
        return NULL;
    }
}

