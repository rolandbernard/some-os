#ifndef _HARTS_H_
#define _HARTS_H_

#include "task/types.h"

#define HART_STACK_SIZE (1 << 16)
#define IDLE_STACK_SIZE 64

extern int hart_lock;
extern int hart_count;
extern int hart_ids[];

void registerHart(int hartid);

int hartIdToIndex(int hartid);

// Create and set a hart frame for the executing hart
HartFrame* setupHartFrame(int hartid);

TrapFrame* readSscratch();

void writeSscratch(TrapFrame* frame);

HartFrame* getCurrentHartFrame();

int readMhartid();

int getCurrentHartId();

void* getKernelGlobalPointer();

#endif
