
#include <stddef.h>
#include <string.h>

#include "process/harts.h"
#include "process/process.h"
#include "memory/kalloc.h"
#include "memory/virtmem.h"

extern void __global_pointer;
extern void __stack_top;

static SpinLock hart_lock;
HartFrame* harts_head = NULL; // This will be a circular linked list
HartFrame* harts_tail = NULL;

HartFrame* setupHartFrame() {
    HartFrame* existing = getCurrentHartFrame();
    if (existing == NULL) {
        HartFrame* hart = kalloc(sizeof(HartFrame));
        lockSpinLock(&hart_lock); 
        hart->next = harts_head;
        harts_head = hart;
        if (harts_tail == NULL) {
            harts_tail = hart;
        } else {
            harts_tail->next = hart;
        }
        unlockSpinLock(&hart_lock); 
        memset(hart, 0, sizeof(HartFrame));
        // Stack_top should be changed for all but the primary hart
        hart->stack_top = &__stack_top;
        initTrapFrame(&hart->frame, (uintptr_t)&__stack_top, (uintptr_t)&__global_pointer, 0, NULL, 0, kernel_page_table);
        writeSscratch(&hart->frame);
        return hart;
    } else {
        return existing;
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

