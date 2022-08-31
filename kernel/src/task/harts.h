#ifndef _HARTS_H_
#define _HARTS_H_

#include <stdnoreturn.h>

#include "task/types.h"

#define HART_STACK_SIZE (1 << 16)
#define IDLE_STACK_SIZE 64

extern int hart_count;
extern int hart_ids[];

void registerHart(int hartid);

int hartIdToIndex(int hartid);

// Create and set a hart frame for the executing hart
HartFrame* setupHartFrame(int hartid);

TrapFrame* readSscratch();

TrapFrame* getCurrentTrapFrame();

void writeSscratch(TrapFrame* frame);

HartFrame* getCurrentHartFrame();

int readMhartid();

int getCurrentHartId();

void* getKernelGlobalPointer();

// Save the current state to frame.
// Returns true immediately, but false when loading the frame.
bool saveToFrame(TrapFrame* frame);

// Load state from frame.
noreturn void loadFromFrame(TrapFrame* frame);

// Save the current state to one TrapFrame and load from the other.
void swapTrapFrame(TrapFrame* load_from, TrapFrame* save_to);

typedef void (*CallInFunction)(TrapFrame* self, ...);

noreturn void callInHart(CallInFunction func, ...);

#endif
