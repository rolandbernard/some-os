
#include <stddef.h>

#include "error/log.h"
#include "interrupt/trap.h"
#include "process/harts.h"
#include "process/process.h"
#include "memory/kalloc.h"
#include "memory/virtmem.h"

extern void __global_pointer;
extern void __stack_top;

SpinLock hart_lock = 0;
HartFrame* harts_head = NULL; // This will be a circular linked list
HartFrame* harts_tail = NULL;

static void idle() {
    for (;;) {
        waitForInterrupt();
    }
}

#define MAX_HART_COUNT 32

int hart_count = 1;
int hart_ids[MAX_HART_COUNT];

int hartIdToIndex(int hartid) {
    for (int i = 0; i < hart_count; i++) {
        if (hart_ids[i] == hartid) {
            return i;
        }
    }
    return hart_count;
}

HartFrame* setupHartFrame(int hartid) {
    HartFrame* existing = getCurrentHartFrame();
    if (existing == NULL) {
        HartFrame* hart = zalloc(sizeof(HartFrame));
        hart->stack_top = kalloc(HART_STACK_SIZE) + HART_STACK_SIZE;
        initTrapFrame(&hart->frame, (uintptr_t)hart->stack_top, (uintptr_t)&__global_pointer, 0, 0, kernel_page_table);
        hart->idle_process = createKernelProcess(idle, LOWEST_PRIORITY, IDLE_STACK_SIZE); // Every hart needs an idle process
        hart->hartid = hartid;
        lockSpinLock(&hart_lock); 
        hart->next = harts_head;
        harts_head = hart;
        if (harts_tail == NULL) {
            harts_tail = hart;
        }
        harts_tail->next = hart;
        writeSscratch(&hart->frame);
        unlockSpinLock(&hart_lock); 
        return hart;
    } else {
        existing->hartid = hartid;
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

int getCurrentHartId() {
    HartFrame* frame = getCurrentHartFrame();
    if (frame != NULL) {
        return frame->hartid;
    } else {
        return readMhartid();
    }
}

