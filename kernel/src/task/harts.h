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

// Note: the saveToFrame and loadFromFrame functions will save/restore only saved registers. For
// saving, nothing else is required, since the ABI ensures all others are saved my the caller before
// calling saveToFrame. For loading, there is loadAllFromFrame and for swapping, there is the
// all_registers argument. Note that this includes floating point registers but not t6.

// Save the current state to frame.
// Returns true immediately, but false when loading the frame.
bool saveToFrame(TrapFrame* frame);

// Load state from frame.
noreturn void loadFromFrame(TrapFrame* frame);

// Load state from frame, including caller saved registers.
noreturn void loadAllFromFrame(TrapFrame* frame);

// Save the current state to one TrapFrame and load from the other.
void swapTrapFrame(TrapFrame* restrict load_from, TrapFrame* restrict save_to, bool all_registers);

typedef void (*CallInFunction)(TrapFrame* self, ...);

noreturn void callInHart(CallInFunction func, ...);

#endif
