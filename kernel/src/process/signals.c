
#include <string.h>

#include "interrupt/timer.h"

#include "process/process.h"
#include "process/signals.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "task/schedule.h"
#include "task/task.h"

static bool shouldIgnoreSignal(Process* process, Signal signal) {
    SignalHandler* action = &process->signals.handlers[signal];
    return signal != SIGKILL && signal != SIGSTOP
           && (
               action->handler == SIG_IGN
               || (
                   action->handler == SIG_DFL
                   && (
                       signal == SIGCHLD || signal == SIGURG || signal == SIGWINCH
                       || signal == SIGUSR1 || signal == SIGUSR2 || signal == SIGCONT
                   )
               )
           );
}

void addSignalToProcess(Process* process, Signal signal, Pid child_pid) {
    if (signal == SIGCONT) {
        // This always continues the precess, even if the signal is blocked/ignored.
        // Is that the correct behaviour?
        continueProcess(process, signal);
    }
    if (signal > SIGNONE && signal < SIG_COUNT && !shouldIgnoreSignal(process, signal)) {
        PendingSignal* entry = kalloc(sizeof(PendingSignal));
        entry->next = NULL;
        entry->signal = signal;
        entry->child_pid = child_pid;
        lockSpinLock(&process->lock);
        if (process->signals.signals_tail == NULL) {
            process->signals.signals_tail = entry;
            process->signals.signals = entry;
        } else {
            process->signals.signals_tail->next = entry;
            process->signals.signals_tail = entry;
        }
        unlockSpinLock(&process->lock);
    }
}

typedef struct {
    int number;
    int code;
    union {
        int integer;
        void *pointer;
    } value;
} SigInfo;

typedef struct {
    uintptr_t regs[31];
    double fregs[32];
    uintptr_t pc;
    uintptr_t mask;
    uintptr_t current_signal;
    uintptr_t restorer_frame;
} SignalRestoreFrame;

static bool handleActualSignal(Task* task, Signal signal) {
    SignalHandler* action = &task->process->signals.handlers[signal];
    if (signal == SIGKILL) {
        exitProcess(task->process, signal, 0);
        return false;
    } else if (signal == SIGSTOP) {
        stopProcess(task->process, signal);
        return false;
    } else if (signal >= SIG_COUNT || signal == SIGNONE) {
        // This is an invalid signal, ignore for now
        return true;
    } else if (action->handler == SIG_DFL) {
        // Execute default action
        if (signal == SIGCHLD || signal == SIGURG || signal == SIGWINCH || signal == SIGUSR1 || signal == SIGUSR2 || signal == SIGCONT) {
            // These are the only signals we ignore
            return true;
        } else if (signal == SIGTSTP || signal == SIGTTIN || signal == SIGTTOU) {
            stopProcess(task->process, signal);
            return false;
        } else {
            exitProcess(task->process, signal, 0);
            return false;
        }
    } else if (action->handler == SIG_IGN) {
        // User wants this to be ignored
        return true;
    } else {
        SigInfo info = {
            .number = signal,
            .code = 0,
            .value = { .integer = 0 },
        };
        // Otherwise enter the given signal handler
        // Copy the current trap frame (to be restored later)
        VirtPtr stack_pointer = virtPtrForTask(task->frame.regs[REG_STACK_POINTER], task);
        // Before these values that will be restored, we can but some other data
        PUSH_TO_VIRTPTR(stack_pointer, info);
        VirtPtr info_address = stack_pointer;
        PUSH_TO_VIRTPTR(stack_pointer, task->process->signals.mask);
        PUSH_TO_VIRTPTR(stack_pointer, task->process->signals.restore_frame);
        PUSH_TO_VIRTPTR(stack_pointer, task->process->signals.current_signal);
        PUSH_TO_VIRTPTR(stack_pointer, task->frame.pc);
        PUSH_TO_VIRTPTR(stack_pointer, task->frame.fregs);
        PUSH_TO_VIRTPTR(stack_pointer, task->frame.regs);
        task->process->signals.mask |= action->mask;
        if ((action->flags & SA_NODEFER) != 0) {
            task->process->signals.mask |= 1UL << (signal - 1);
        }
        task->process->signals.restore_frame = stack_pointer.address;
        task->process->signals.current_signal = signal;
        task->frame.pc = action->handler;
        // Set argument to signal type
        task->frame.regs[REG_ARGUMENT_0] = signal;
        task->frame.regs[REG_ARGUMENT_1] = info_address.address;
        task->frame.regs[REG_ARGUMENT_2] = stack_pointer.address;
        // Set stack pointer to after the restore frame
        task->frame.regs[REG_STACK_POINTER] = stack_pointer.address;
        // Set return address to the restorer to be called after handler completion
        task->frame.regs[REG_RETURN_ADDRESS] = task->process->signals.handlers[signal].restorer;
        if ((action->flags & SA_RESETHAND) != 0) {
            action->handler = SIG_DFL;
            action->flags &= ~SA_SIGINFO;
        }
        return true;
    }
}

static Signal getSignalToHandle(Task* task) {
    PendingSignal* previous = NULL;
    PendingSignal** current = &task->process->signals.signals;
    while (*current != NULL) {
        Signal signal = (*current)->signal;
        if (
            signal == SIGKILL || signal == SIGSTOP
            || (task->process->signals.mask & (1UL << (signal - 1))) == 0
        ) {
            PendingSignal* pending = *current;
            *current = pending->next;
            if (task->process->signals.signals_tail == pending) {
                task->process->signals.signals_tail = previous;
            }
            dealloc(pending);
            return signal;
        } else {
            previous = *current;
            current = &previous->next;
        }
    }
    return SIGNONE;
}

bool handlePendingSignals(Task* task) {
    bool ret = true;
    lockSpinLock(&task->process->lock);
    Signal signal = getSignalToHandle(task);
    if (signal != SIGNONE) {
        ret = handleActualSignal(task, signal);
    } else if (task->process->signals.alarm_at != 0 && getTime() >= task->process->signals.alarm_at) {
        ret = handleActualSignal(task, SIGALRM);
        task->process->signals.alarm_at = 0;
    }
    unlockSpinLock(&task->process->lock);
    return ret;
}

void returnFromSignal(Task* task) {
    lockSpinLock(&task->process->lock);
    if (task->process->signals.current_signal != SIGNONE) {
        VirtPtr stack_pointer = virtPtrForTask(task->process->signals.restore_frame, task);
        POP_FROM_VIRTPTR(stack_pointer, task->frame.regs);
        POP_FROM_VIRTPTR(stack_pointer, task->frame.fregs);
        POP_FROM_VIRTPTR(stack_pointer, task->frame.pc);
        POP_FROM_VIRTPTR(stack_pointer, task->process->signals.current_signal);
        POP_FROM_VIRTPTR(stack_pointer, task->process->signals.restore_frame);
        POP_FROM_VIRTPTR(stack_pointer, task->process->signals.mask);
    } // If we are not in a handle do nothing
    unlockSpinLock(&task->process->lock);
}

void clearSignals(Process* process) {
    lockSpinLock(&process->lock);
    while (process->signals.signals != NULL) {
        PendingSignal* signal = process->signals.signals;
        process->signals.signals = signal->next;
        dealloc(signal);
    }
    process->signals.signals_tail = NULL;
    process->signals.altstack = 0;
    process->signals.current_signal = 0;
    process->signals.restore_frame = 0;
    for (size_t i = 0; i < SIG_COUNT; i++) {
        process->signals.handlers[i].flags = 0;
        if (process->signals.handlers[i].handler != SIG_IGN) {
            process->signals.handlers[i].handler = SIG_DFL;
        }
    }
    unlockSpinLock(&process->lock);
}

void clearPendingChildSignals(Process* process, Pid child_pid) {
    lockSpinLock(&process->lock);
    PendingSignal** current = &process->signals.signals;
    while (*current != NULL) {
        PendingSignal* signal = *current;
        if (signal->signal == SIGCHLD && signal->child_pid == child_pid) {
            *current = signal->next;
            dealloc(signal);
        } else {
            current = &signal->next;
        }
    }
    if (process->signals.signals == NULL) {
        process->signals.signals_tail = NULL;
    }
    unlockSpinLock(&process->lock);
}

