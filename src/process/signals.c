
#include <string.h>

#include "process/signals.h"

#include "memory/kalloc.h"

void addSignalToProcess(Process* process, Signal signal) {
    lockSpinLock(&process->signals.lock);
    size_t size = process->signals.signal_count;
    process->signals.signals = krealloc(process->signals.signals, (size + 1) * sizeof(Signal));
    process->signals.signals[size] = signal;
    process->signals.signal_count++;
    unlockSpinLock(&process->signals.lock);
}

static void handleActualSignal(Process* process, int signal) {
    if (signal >= SIG_COUNT || signal == 0) {
        // This is an invalid signal, ignore for now
    } else if (process->signals.handlers[signal].handler == SIG_IGN) {
        // User wants this to be ignored
    } else if (process->signals.handlers[signal].handler == SIG_DFL) {
        // Execute default action
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
        memcpy(&process->frame, &process->signals.suspended, sizeof(TrapFrame));
        process->signals.in_handler = false;
    } // If we are not in a handle do nothing
    unlockSpinLock(&process->signals.lock);
}

