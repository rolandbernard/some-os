#ifndef _PROCESS_TYPES_H_
#define _PROCESS_TYPES_H_

// This file combines some of the types in one file to avoid recursive imports

#include <stdint.h>
#include <stddef.h>

#include "memory/pagetable.h"
#include "util/spinlock.h"
#include "files/vfs.h"

struct HartFrame_s;

typedef enum { // This is offset by one to be used as the index into TrapFrame.regs
    REG_RETURN_ADDRESS = 0,
    REG_STACK_POINTER = 1,
    REG_GLOBAL_POINTER = 2,
    REG_THREAD_POINTER = 3,
    REG_TEMP_0 = 4,
    REG_TEMP_1 = 5,
    REG_TEMP_2 = 6,
    REG_SAVED_0 = 7,
    REG_SAVED_1 = 8,
    REG_ARGUMENT_0 = 9,
    REG_ARGUMENT_1 = 10,
    REG_ARGUMENT_2 = 11,
    REG_ARGUMENT_3 = 12,
    REG_ARGUMENT_4 = 13,
    REG_ARGUMENT_5 = 14,
    REG_ARGUMENT_6 = 15,
    REG_ARGUMENT_7 = 16,
    REG_SAVED_2 = 17,
    REG_SAVED_3 = 18,
    REG_SAVED_4 = 19,
    REG_SAVED_5 = 20,
    REG_SAVED_6 = 21,
    REG_SAVED_7 = 22,
    REG_SAVED_8 = 23,
    REG_SAVED_9 = 24,
    REG_SAVED_10 = 25,
    REG_SAVED_11 = 26,
    REG_TEMP_3 = 27,
    REG_TEMP_4 = 28,
    REG_TEMP_5 = 29,
    REG_TEMP_6 = 30,
} RegisterNames;

typedef struct {
    // Be careful changing this. It's used from assembly
    struct HartFrame_s* hart;
    uintptr_t regs[31];
    double fregs[32];
    uintptr_t pc;
    uintptr_t satp;
} TrapFrame;

#define MAX_PRIORITY 40

#define HIGHEST_PRIORITY 0
#define LOWEST_PRIORITY (MAX_PRIORITY - 1)
#define DEFAULT_PRIORITY (MAX_PRIORITY / 2)

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
    ENQUEUEABLE,
    RUNNING,
    READY,
    WAITING,
    SLEEPING,
    TERMINATED, // Still has resources
    FREED, // Resources have been freed
} ProcessState;

typedef int Pid;
typedef uint8_t Priority;

typedef struct {
    Priority priority;
    Priority queue_priority; // Is a maximum of priority, but will be decreased over time
    uint16_t runs;
    ProcessState state;
    struct Process_s* sched_next; // Used for ready and waiting lists
} ProcessScheduling;

typedef struct {
    PageTable* table;
    void* stack; // This is only used for a kernel process
} ProcessMemory;

typedef struct {
    Uid uid;
    Gid gid;
    int next_fd;
    size_t fd_count;
    int* fds;
    VfsFile** files;
} ProcessResources;

typedef struct {
    struct Process_s* parent;
    struct Process_s* children;
    struct Process_s* child_next;
    struct Process_s* child_prev;
    struct Process_s* global_next; // Used for list off every process
    struct Process_s* global_prev;
} ProcessTree;

typedef struct Process_s {
    TrapFrame frame;
    Pid pid;
    uint64_t status; // This is the status returned from wait
    ProcessTree tree;
    ProcessScheduling sched;
    ProcessMemory memory;
    ProcessResources resources;
} Process;

#endif
