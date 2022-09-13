
#include <stdint.h>

#include "interrupt/com.h"

#include "error/log.h"
#include "error/panic.h"
#include "kernel/kernel.h"
#include "task/harts.h"
#include "task/schedule.h"
#include "task/syscall.h"
#include "util/unsafelock.h"

// Locked by sender, unlocked by receiver
static UnsafeLock message_write_lock;
static UnsafeLock message_read_lock;
static MessageType message_type;
static void* message_data;

void sendMessageTo(int hartid, MessageType type, void* data) {
    Task* task = criticalEnter();
    lockUnsafeLock(&message_write_lock);
    message_type = type;
    message_data = data;
    lockUnsafeLock(&message_read_lock);
    sendMachineSoftwareInterrupt(hartid);
    // Wait for the receiver to unlock the read lock
    lockUnsafeLock(&message_read_lock);
    unlockUnsafeLock(&message_read_lock);
    // Only after the message has been handled can we write another one
    unlockUnsafeLock(&message_write_lock);
    criticalReturn(task);
}

void sendMessageToAll(MessageType type, void* data) {
    int hartid = getCurrentHartId();
    for (int i = 0; i < hart_count; i++) {
        if (hart_ids[i] != hartid) {
            sendMessageTo(hart_ids[i], type, data);
        }
    }
}

void sendMessageToSelf(MessageType type, void* data) {
    sendMessageTo(getCurrentHartId(), type, data);
}

void handleMessage(MessageType type, void* data) {
    if (type == INITIALIZE_HARTS) {
        unlockUnsafeLock(&message_read_lock);
        initHart(getCurrentHartId());
        runNextTask();
    } else if (type == NONE || type == YIELD_TASK) {
        unlockUnsafeLock(&message_read_lock);
        // Do nothing, the point was to preempt the running process
    } else if (type == KERNEL_PANIC) {
        unlockUnsafeLock(&message_read_lock);
        silentPanic();
    } else {
        panic();
    }
}

void handleMachineSoftwareInterrupt() {
    int hartid = getCurrentHartId();
    clearMachineSoftwareInterrupt(hartid);
    handleMessage(message_type, message_data);
}

