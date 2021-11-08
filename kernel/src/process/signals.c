
#include <string.h>

#include "interrupt/timer.h"
#include "process/signals.h"

#include "error/log.h"
#include "memory/kalloc.h"
#include "process/schedule.h"

void addSignalToProcess(Process* process, Signal signal) {
    PendingSignal* entry = kalloc(sizeof(PendingSignal));
    entry->signal = signal;
    entry->next = NULL;
    lockSpinLock(&process->signals.lock);
    if (process->signals.signals_tail == NULL) {
        process->signals.signals_tail = entry;
        process->signals.signals = entry;
    } else {
        process->signals.signals_tail->next = entry;
        process->signals.signals_tail = entry;
    }
    unlockSpinLock(&process->signals.lock);
}

typedef struct {
    uintptr_t regs[31];
    double fregs[32];
    uintptr_t pc;
    uintptr_t mask;
    uintptr_t current_signal;
    uintptr_t restorer_frame;
} SignalRestoreFrame;

static void handleActualSignal(Process* process, Signal signal) {
    if (signal == SIGKILL || signal == SIGSTOP) {
        // We don't actually have a stop at the moment
        process->status = (process->status & ~0xff00) | ((signal & 0xff) << 8);
        moveToSchedState(process, TERMINATED);
    } else if (signal >= SIG_COUNT || signal == 0) {
        // This is an invalid signal, ignore for now
    } else if (process->signals.handlers[signal] == SIG_DFL) {
        // Execute default action
        if (signal == SIGCHLD || signal == SIGURG || signal == SIGWINCH) {
            // These are the only signals we ignore
        } else {
            process->status = (process->status & ~0xff00) | ((signal & 0xff) << 8);
            moveToSchedState(process, TERMINATED);
        }
    } else if (process->signals.handlers[signal] == SIG_IGN) {
        // User wants this to be ignored
    } else {
        // Otherwise enter the given signal handler
        // Copy the current trap frame (to be restored later)
        VirtPtr stack_pointer = virtPtrFor(process->frame.regs[REG_STACK_POINTER], process->memory.mem);
        PUSH_TO_VIRTPTR(stack_pointer, process->signals.mask);
        PUSH_TO_VIRTPTR(stack_pointer, process->signals.restore_frame);
        PUSH_TO_VIRTPTR(stack_pointer, process->signals.current_signal);
        PUSH_TO_VIRTPTR(stack_pointer, process->frame.pc);
        PUSH_TO_VIRTPTR(stack_pointer, process->frame.fregs);
        PUSH_TO_VIRTPTR(stack_pointer, process->frame.regs);
        process->signals.mask |= process->signals.handlers[signal]->mask;
        if ((process->signals.handlers[signal]->flags & SA_NODEFER) != 0) {
            process->signals.mask |= 1UL << (signal - 1);
        }
        process->signals.restore_frame = stack_pointer.address;
        process->signals.current_signal = signal;
        process->frame.pc = process->signals.handlers[signal]->handler;
        // Set argument to signal type
        process->frame.regs[REG_ARGUMENT_0] = signal;
        // Set stack pointer to after the restore frame
        process->frame.regs[REG_STACK_POINTER] = stack_pointer.address;
        // Set return address to the restorer to be called after handler completion
        process->frame.regs[REG_RETURN_ADDRESS] = process->signals.handlers[signal]->restorer;
    }
}

static Signal getSignalToHandle(Process* process) {
    PendingSignal* previous = NULL;
    PendingSignal** current = &process->signals.signals;
    while (*current != NULL) {
        Signal signal = (*current)->signal;
        if (
            signal == SIGKILL || signal == SIGSTOP
            || (process->signals.mask & (1UL << (signal - 1))) == 0
        ) {
            PendingSignal* pending = *current;
            *current = pending->next;
            if (process->signals.signals_tail == pending) {
                process->signals.signals_tail = previous;
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

void handlePendingSignals(Process* process) {
    lockSpinLock(&process->signals.lock);
    Signal signal = getSignalToHandle(process);
    if (signal != SIGNONE) {
        handleActualSignal(process, signal);
    } else if (process->signals.alarm_at != 0 && getTime() >= process->signals.alarm_at) {
        handleActualSignal(process, SIGALRM);
        process->signals.alarm_at = 0;
    }
    unlockSpinLock(&process->signals.lock);
}

void returnFromSignal(Process* process) {
    lockSpinLock(&process->signals.lock);
    if (process->signals.current_signal != SIGNONE) {
        VirtPtr stack_pointer = virtPtrFor(process->signals.restore_frame, process->memory.mem);
        POP_FROM_VIRTPTR(stack_pointer, process->frame.regs);
        POP_FROM_VIRTPTR(stack_pointer, process->frame.fregs);
        POP_FROM_VIRTPTR(stack_pointer, process->frame.pc);
        POP_FROM_VIRTPTR(stack_pointer, process->signals.current_signal);
        POP_FROM_VIRTPTR(stack_pointer, process->signals.restore_frame);
        POP_FROM_VIRTPTR(stack_pointer, process->signals.mask);
    } // If we are not in a handle do nothing
    unlockSpinLock(&process->signals.lock);
}
