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

struct HartProcess_s;

typedef struct {
    // Be careful changing this. It's used from assembly
    struct HartProcess_s* hart;
    uintptr_t regs[31];
    double fregs[32];
    uintptr_t pc;
    uintptr_t satp;
} TrapFrame;

typedef struct {
    TrapFrame frame;
    bool is_kernel;
    uintptr_t stack_top;
    uintptr_t globals;
    ProcessState state;
    uint64_t pid;
    PageTable* table;
} Process;

typedef struct {
    Process process;
    uint64_t schedule_id;
} HartProcess;

// Initialize process system
Error initProcessSystem();

// Initialize a process for the given stack_top, globals and start pc
void initProcess(Process* process, uintptr_t stack_top, uintptr_t globals, uintptr_t start);

// Enter process into the user ot kernel mode depending on process.is_kernel
void enterProcess(Process* process);

#endif
