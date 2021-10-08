
#include <string.h>

#include "process/signals.h"

#include "error/log.h"
#include "memory/kalloc.h"
#include "process/schedule.h"

void addSignalToProcess(Process* process, Signal signal) {
    lockSpinLock(&process->signals.lock);
    size_t size = process->signals.signal_count;
    process->signals.signals = krealloc(process->signals.signals, (size + 1) * sizeof(Signal));
    process->signals.signals[size] = signal;
    process->signals.signal_count++;
    unlockSpinLock(&process->signals.lock);
}

static void handleActualSignal(Process* process, int signal) {
    if (signal == SIGKILL || signal == SIGSTOP) {
        // We don't actually have a stop at the moment
        process->status = (process->status & ~0xff00) | ((signal & 0xff) << 8);
        moveToSchedState(process, TERMINATED);
    }
    if (signal >= SIG_COUNT || signal == 0) {
        // This is an invalid signal, ignore for now
    } else if (process->signals.handlers[signal].handler == SIG_IGN) {
        // User wants this to be ignored
    } else if (process->signals.handlers[signal].handler == SIG_DFL) {
        // Execute default action
        if (signal == SIGCHLD || signal == SIGURG || signal == SIGWINCH) {
            // These are the only signals we ignore
        } else {
            process->status = (process->status & ~0xff00) | ((signal & 0xff) << 8);
            moveToSchedState(process, TERMINATED);
        }
    } else {
        // Otherwise enter the given signal handler
        // Copy the current trap frame (to be restored later)
        memcpy(&process->signals.suspended, &process->frame, sizeof(TrapFrame));
        if (process->signals.stack != 0) {
            // If using an alternate signal stack
            process->frame.regs[REG_STACK_POINTER] = process->signals.stack;
        }
        process->frame.pc = process->signals.handlers[signal].handler;
        process->frame.regs[REG_ARGUMENT_0] = signal;
        // Set return address to the restorer to be called after handler completion
        process->frame.regs[REG_RETURN_ADDRESS] = process->signals.handlers[signal].restorer;
        process->signals.in_handler = true;
    }
}

void handlePendingSignals(Process* process) {
    lockSpinLock(&process->signals.lock);
    if (!process->signals.in_handler) {
        if (process->signals.signal_count != 0) {
            Signal sig = process->signals.signals[0];
            process->signals.signal_count--;
            memmove(process->signals.signals, process->signals.signals + 1, process->signals.signal_count);
            handleActualSignal(process, sig);
        }
    } // If we are already in a handler, don't do anything
    unlockSpinLock(&process->signals.lock);
}

void returnFromSignal(Process* process) {
    lockSpinLock(&process->signals.lock);
    if (process->signals.in_handler) {
        // Reload the original state
        uintptr_t satp = process->frame.satp; // Keep satp and hart
        HartFrame* hart = process->frame.hart;
        memcpy(&process->frame, &process->signals.suspended, sizeof(TrapFrame));
        process->frame.satp = satp;
        process->frame.hart = hart;
        process->signals.in_handler = false;
    } // If we are not in a handle do nothing
    unlockSpinLock(&process->signals.lock);
}

