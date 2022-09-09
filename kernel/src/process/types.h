#ifndef _PROCESS_TYPES_H_
#define _PROCESS_TYPES_H_

// This file combines some of the types in one file to avoid recursive imports

#include <stdint.h>
#include <stddef.h>

#include "memory/pagetable.h"
#include "memory/memspace.h"
#include "task/spinlock.h"
#include "files/vfs/types.h"
#include "task/task.h"

typedef int Pid;

typedef struct {
    MemorySpace* mem;
    uintptr_t start_brk;
    uintptr_t brk;
} ProcessMemory;

typedef struct {
    VfsMode umask;
    VfsFileDescriptor* files;
    char* cwd;
    TaskLock lock;
} ProcessResources;

typedef struct {
    Uid ruid;
    Gid rgid;
    Uid suid;
    Gid sgid;
    Uid euid;
    Gid egid;
    SpinLock lock;
} ProcessUser;

typedef struct {
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
    uintptr_t restorer;
    SignalSet mask;
    SigActionFlags flags;
} SignalHandler;

typedef struct PendingSignal_s {
    struct PendingSignal_s* next;
    Signal signal;
} PendingSignal;

typedef struct {
    PendingSignal* signals;
    PendingSignal* signals_tail;
    SignalHandler handlers[SIG_COUNT];
    Signal current_signal;
    uintptr_t restore_frame;
    Time alarm_at;
    uintptr_t altstack;
    SignalSet mask;
} ProcessSignals;

typedef struct Process_s {
    SpinLock lock;
    Pid pid;
    Pid sid;
    Pid pgid;
    int status; // This is the status returned from wait
    Task* tasks;
    TaskTimes times;
    ProcessUser user;
    ProcessTree tree;
    ProcessMemory memory;
    ProcessResources resources;
    ProcessSignals signals;
} Process;

#endif
