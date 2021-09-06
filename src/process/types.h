#ifndef _PROCESS_TYPES_H_
#define _PROCESS_TYPES_H_

// This file combines some of the types in one file to avoid recursive imports

#include <stdint.h>

#include "memory/pagetable.h"
#include "util/spinlock.h"

struct HartFrame_s;

typedef struct {
    // Be careful changing this. It's used from assembly
    struct HartFrame_s* hart;
    uintptr_t regs[31];
    double fregs[32];
    uintptr_t pc;
    uintptr_t satp;
} TrapFrame;

#define MAX_PRIORITY 40

struct Process_s;

typedef struct {
    SpinLock lock;
    struct Process_s* head;
    struct Process_s* tails[MAX_PRIORITY];
} ScheduleQueue;

typedef struct HartFrame_s {
    TrapFrame frame;
    void* stack_top;
    ScheduleQueue queue;
    struct Process_s* idle_process;
    struct HartFrame_s* next; // Next hart. Used for scheduling
} HartFrame;

typedef enum {
    RUNNING,
    READY,
    WAITING,
    TERMINATED,
    KILLED,
} ProcessState;

typedef uint64_t Pid;
typedef uint8_t Priority;

typedef struct Process_s {
    TrapFrame frame;
    uint64_t status; // This is the status returned from wait
    Pid pid;
    PageTable* table;
    Priority priority;
    ProcessState state;
    void* stack; // This is only used for a kernel process
    struct Process_s* parent;
    struct Process_s* children;
    struct Process_s* child_next;
    struct Process_s* sched_next; // Used for ready and waiting lists
    struct Process_s* global_next; // Used for list off every process
    struct Process_s* global_prev;
} Process;

#endif
