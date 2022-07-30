
#include <string.h>

#include "interrupt/timer.h"

#include "process/process.h"
#include "process/signals.h"
#include "error/log.h"
#include "memory/kalloc.h"
#include "task/schedule.h"
#include "task/task.h"

void addSignalToProcess(Process* process, Signal signal) {
    PendingSignal* entry = kalloc(sizeof(PendingSignal));
    entry->signal = signal;
    entry->next = NULL;
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

typedef struct {
    uintptr_t regs[31];
    double fregs[32];
    uintptr_t pc;
    uintptr_t mask;
    uintptr_t current_signal;
    uintptr_t restorer_frame;
} SignalRestoreFrame;

static bool handleActualSignal(Task* task, Signal signal) {
    if (signal == SIGKILL || signal == SIGSTOP) {
        // We don't actually have a stop at the moment
        exitProcess(task->process, signal, 0);
        return false;
    } else if (signal >= SIG_COUNT || signal == 0) {
        // This is an invalid signal, ignore for now
        return true;
    } else if (task->process->signals.handlers[signal] == SIG_DFL) {
        // Execute default action
        if (signal == SIGCHLD || signal == SIGURG || signal == SIGWINCH || signal == SIGUSR1 || signal == SIGUSR2) {
            // These are the only signals we ignore
            return true;
        } else {
            exitProcess(task->process, signal, 0);
            return false;
        }
    } else if (task->process->signals.handlers[signal] == SIG_IGN) {
        // User wants this to be ignored
        return true;
    } else {
        // Otherwise enter the given signal handler
        // Copy the current trap frame (to be restored later)
        VirtPtr stack_pointer = virtPtrForTask(task->frame.regs[REG_STACK_POINTER], task);
        PUSH_TO_VIRTPTR(stack_pointer, task->process->signals.mask);
        PUSH_TO_VIRTPTR(stack_pointer, task->process->signals.restore_frame);
        PUSH_TO_VIRTPTR(stack_pointer, task->process->signals.current_signal);
        PUSH_TO_VIRTPTR(stack_pointer, task->frame.pc);
        PUSH_TO_VIRTPTR(stack_pointer, task->frame.fregs);
        PUSH_TO_VIRTPTR(stack_pointer, task->frame.regs);
        task->process->signals.mask |= task->process->signals.handlers[signal]->mask;
        if ((task->process->signals.handlers[signal]->flags & SA_NODEFER) != 0) {
            task->process->signals.mask |= 1UL << (signal - 1);
        }
        task->process->signals.restore_frame = stack_pointer.address;
        task->process->signals.current_signal = signal;
        task->frame.pc = task->process->signals.handlers[signal]->handler;
        // Set argument to signal type
        task->frame.regs[REG_ARGUMENT_0] = signal;
        // Set stack pointer to after the restore frame
        task->frame.regs[REG_STACK_POINTER] = stack_pointer.address;
        // Set return address to the restorer to be called after handler completion
        task->frame.regs[REG_RETURN_ADDRESS] = task->process->signals.handlers[signal]->restorer;
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

