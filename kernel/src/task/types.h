#ifndef _TASK_TYPES_H_
#define _TASK_TYPES_H_

#include <stdint.h>
#include <stddef.h>

#include "util/spinlock.h"
#include "interrupt/timer.h"

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

struct Task_s;

typedef struct {
    SpinLock lock;
    struct Task_s* head;
    struct Task_s* tails[MAX_PRIORITY];
} ScheduleQueue;

typedef struct HartFrame_s {
    TrapFrame frame;
    void* stack_top;
    int hartid;
    ScheduleQueue queue;
    struct Task_s* idle_task;
    struct HartFrame_s* next; // Next hart. Used for scheduling
} HartFrame;

typedef enum {
    UNKNOWN = 0,
    RUNNING,
    READY,
    WAITING,
    SLEEPING,
    PAUSED,
    TERMINATED,
    // Special
    WAIT_CHLD,
} TaskState;

typedef uint8_t Priority;

typedef struct {
    // All data needed for scheduling
    Priority priority;
    Priority queue_priority; // Is at maximum priority, but will be decreased over time
    uint16_t runs;
    TaskState state;
    struct Task_s* sched_next; // Used for ready and waiting lists
    Time sleeping_until;
} TaskSched;

typedef struct {
    // Data for resource monitoring
    Time entered;
    Time user_time;
    Time user_child_time;
    Time system_time;
    Time system_child_time;
} TaskTimes;

struct Process_s;

typedef struct Task_s {
    TrapFrame frame;
    void* stack;
    uintptr_t stack_top;
    TaskSched sched;
    TaskTimes times;
    struct Process_s* process;
    struct Task_s* proc_next;
} Task;

#endif
