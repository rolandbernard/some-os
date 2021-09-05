
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "interrupt/syscall.h"
#include "process/harts.h"
#include "process/process.h"
#include "process/syscall.h"
#include "process/schedule.h"
#include "process/types.h"

uintptr_t exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    KERNEL_LOG("Exit");
    Process* process = (Process*)frame;
    process->state = TERMINATED;
    return 0;
}

uintptr_t yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    // If this is a process. Calling a syscall will yield anyways.
    return 0;
}

void exit() {
    syscall(SYSCALL_EXIT);
    panic(); // This will never return
}

void yield() {
    if (getCurrentProcess() != NULL) {
        // If in a process the only thing needed is calling the syscall
        syscall(SYSCALL_YIELD);
    } else {
        // TODO: How to fix deadlock?
        // This means calls to 
    }
}

