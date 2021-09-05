#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <stdint.h>

#include "error/error.h"
#include "memory/pagetable.h"

typedef enum {
    RUNNING,
    READY,
    WAITING,
    TERMINATED,
} ProcessState;

typedef uint64_t Pid;

struct HartFrame_s;

typedef struct {
    // Be careful changing this. It's used from assembly
    struct HartFrame_s* hart;
    uintptr_t regs[31];
    double fregs[32];
    uintptr_t pc;
    uintptr_t satp;
} TrapFrame;

typedef struct {
    TrapFrame frame;
    ProcessState state;
    Pid pid;
    PageTable* table;
} Process;

typedef struct HartFrame_s {
    TrapFrame frame;
    void* globals;
    void* stack_top;
} HartFrame;

// Initialize process system
Error initProcessSystem();

void initTrapFrame(TrapFrame* frame, uintptr_t sp, uintptr_t gp, uintptr_t pc, HartFrame* hart, uintptr_t asid, PageTable* table);

// Initialize a process for the given stack_top, globals and start pc
void initDefaultProcess(Process* process, uintptr_t stack_top, uintptr_t globals, uintptr_t start);

// Enter process into the user ot kernel mode depending on process.pid (pid == 0 -> kernel)
void enterProcess(Process* process);

TrapFrame* readSscratch();

void writeSscratch(TrapFrame* frame);

#endif
