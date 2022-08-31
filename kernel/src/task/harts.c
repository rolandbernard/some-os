
#include <assert.h>
#include <stddef.h>

#include "interrupt/trap.h"
#include "task/harts.h"
#include "task/task.h"
#include "util/unsafelock.h"
#include "memory/kalloc.h"
#include "memory/virtmem.h"

extern char __global_pointer[];

UnsafeLock hart_lock;
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
    assert(existing == NULL);
    HartFrame* hart = zalloc(sizeof(HartFrame));
    hart->stack_top = (uintptr_t)kalloc(HART_STACK_SIZE) + HART_STACK_SIZE;
    initKernelTrapFrame(&hart->frame, hart->stack_top, 0);
    hart->idle_task = createKernelTask(idle, IDLE_STACK_SIZE, LOWEST_PRIORITY); // Every hart needs an idle process
    hart->hartid = hartid;
    writeSscratch(&hart->frame);
    lockUnsafeLock(&hart_lock); 
    hart->next = harts_head;
    harts_head = hart;
    if (harts_tail == NULL) {
        harts_tail = hart;
    }
    harts_tail->next = hart;
    unlockUnsafeLock(&hart_lock); 
    return hart;
}

HartFrame* getCurrentHartFrame() {
    TrapFrame* frame = readSscratch();
    if (frame != NULL && frame->hart != NULL) {
        return frame->hart;
    } else {
        return (HartFrame*)frame;
    }
}

TrapFrame* getCurrentTrapFrame() {
    return readSscratch();
}

void* getKernelGlobalPointer() {
    return __global_pointer;
}

int getCurrentHartId() {
    HartFrame* frame = getCurrentHartFrame();
    if (frame != NULL) {
        return frame->hartid;
    } else {
        return readMhartid();
    }
}

void swapTrapFrame(TrapFrame* load_from, TrapFrame* save_to) {
    if (saveToFrame(save_to)) {
        loadFromFrame(load_from);
    }
}

