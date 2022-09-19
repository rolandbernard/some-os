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

// Most of these are actually never used (they are copied from newlib)
typedef enum {
    SIGNONE = 0,
    SIGHUP  = 1,    /* hangup */
    SIGINT  = 2,    /* interrupt */
    SIGQUIT = 3,    /* quit */
    SIGILL  = 4,    /* illegal instruction (not reset when caught) */
    SIGTRAP = 5,    /* trace trap (not reset when caught) */
    SIGIOT  = 6,    /* IOT instruction */
    SIGABRT = 6,    /* used by abort, replace SIGIOT in the future */
    SIGEMT  = 7,    /* EMT instruction */
    SIGFPE  = 8,    /* floating point exception */
    SIGKILL = 9,    /* kill (cannot be caught or ignored) */
    SIGBUS  = 10,   /* bus error */
    SIGSEGV = 11,   /* segmentation violation */
    SIGSYS  = 12,   /* bad argument to system call */
    SIGPIPE = 13,   /* write on a pipe with no one to read it */
    SIGALRM = 14,   /* alarm clock */
    SIGTERM = 15,   /* software termination signal from kill */
    SIGURG  = 16,   /* urgent condition on IO channel */
    SIGSTOP = 17,   /* sendable stop signal not from tty */
    SIGTSTP = 18,   /* stop signal from tty */
    SIGCONT = 19,   /* continue a stopped process */
    SIGCHLD = 20,   /* to parent on child stop or exit */
    SIGCLD  = 20,   /* System V name for SIGCHLD */
    SIGTTIN = 21,   /* to readers pgrp upon background tty read */
    SIGTTOU = 22,   /* like TTIN for output if (tp->t_local&LTOSTOP) */
    SIGIO   = 23,   /* input/output possible signal */
    SIGPOLL = SIGIO,/* System V name for SIGIO */
    SIGXCPU = 24,   /* exceeded CPU time limit */
    SIGXFSZ = 25,   /* exceeded file size limit */
    SIGVTALRM = 26, /* virtual time alarm */
    SIGPROF = 27,   /* profiling time alarm */
    SIGWINCH = 28,  /* window changed */
    SIGLOST = 29,   /* resource lost (eg, record-lock lost) */
    SIGUSR1 = 30,   /* user defined signal 1 */
    SIGUSR2 = 31,   /* user defined signal 2 */
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
    SigActionFlags flags;
    uintptr_t restorer;
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
