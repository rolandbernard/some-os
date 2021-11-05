#ifndef _PROCESS_TYPES_H_
#define _PROCESS_TYPES_H_

// This file combines some of the types in one file to avoid recursive imports

#include <stdint.h>
#include <stddef.h>

#include "memory/pagetable.h"
#include "memory/memspace.h"
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
    int hartid;
    ScheduleQueue queue;
    struct Process_s* idle_process;
    struct HartFrame_s* next; // Next hart. Used for scheduling
} HartFrame;

typedef enum {
    ENQUEUEABLE,
    RUNNING,
    READY,
    WAITING,
    WAIT_CHLD,
    SLEEPING,
    PAUSED,
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
    Time sleeping_until;
} ProcessScheduling;

typedef struct {
    MemorySpace* mem;
    void* stack; // This is only used for a kernel process
    uintptr_t start_brk;
    uintptr_t brk;
} ProcessMemory;

typedef struct {
    VfsFile* file;
} ProcessFileDescEntry;

typedef struct {
    Uid uid;
    Gid gid;
    int next_fd;
    VfsFile* files;
    char* cwd;
} ProcessResources;

typedef struct ProcessWaitResult_s {
    int pid;
    int status;
    Time user_time;
    Time system_time;
    struct ProcessWaitResult_s* next;
} ProcessWaitResult;

typedef struct {
    ProcessWaitResult* waits;
    struct Process_s* parent;
    struct Process_s* children;
    struct Process_s* child_next; // Next and prev to be able to remove only knowing the process we want to remove
    struct Process_s* child_prev;
    struct Process_s* global_next; // Used for list off every process
    struct Process_s* global_prev;
} ProcessTree;

// Most of these are actually never used
typedef enum {
    SIGNONE = 0,
    SIGHUP = 1,
    SIGINT,
    SIGQUIT,
    SIGILL,
    SIGTRAP,
    SIGABRT,
    SIGIOT = SIGABRT,
    SIGBUS,
    SIGEMT,
    SIGFPE,
    SIGKILL,
    SIGUSR1,
    SIGSEGV,
    SIGUSR2,
    SIGPIPE,
    SIGALRM,
    SIGTERM,
    SIGSTKFLT,
    SIGCHLD,
    SIGCLD = SIGCHLD,
    SIGCONT,
    SIGSTOP,
    SIGTSTP,
    SIGTTIN,
    SIGTTOU,
    SIGURG,
    SIGXCPU,
    SIGXFSZ,
    SIGVTALRM,
    SIGPROF,
    SIGWINCH,
    SIGIO,
    SIGPOLL = SIGIO,
    SIGPWR,
    SIGINFO = SIGPWR,
    SIGLOST,
    SIGSYS,
    SIGUNUSED = SIGSYS,
    SIG_COUNT,
} Signal;

typedef uint64_t SignalSet;

// Most of these are not supported
typedef enum {
    SA_NOCLDSTOP = (1 << 0),
    SA_ONSTACK = (1 << 1),
    SA_RESETHAND = (1 << 2),
    SA_RESTART = (1 << 3),
    SA_SIGINFO = (1 << 4),
    SA_NOCLDWAIT = (1 << 5),
    SA_NODEFER = (1 << 6),
} SigActionFlags;

typedef struct {
    uintptr_t handler;
    SignalSet mask;
    int flags;
    uintptr_t sigaction;
    uintptr_t restorer;
} SignalHandler;

typedef struct PendingSignal_s {
    struct PendingSignal_s* next;
    Signal signal;
} PendingSignal;

typedef struct {
    SpinLock lock; // Signals can be sent by other processes, so we need a lock
    PendingSignal* signals;
    PendingSignal* signals_tail;
    SignalHandler* handlers[SIG_COUNT];
    Signal current_signal;
    uintptr_t restore_frame;
    Time alarm_at;
    uintptr_t altstack;
    SignalSet mask;
} ProcessSignals;

typedef struct {
    Time entered;
    Time user_time;
    Time user_child_time;
    Time system_time;
    Time system_child_time;
} ProcessTimes;

typedef struct Process_s {
    TrapFrame frame;
    Pid pid;
    int status; // This is the status returned from wait
    ProcessTree tree;
    ProcessScheduling sched;
    ProcessMemory memory;
    ProcessResources resources;
    ProcessSignals signals;
    ProcessTimes times;
} Process;

#endif
