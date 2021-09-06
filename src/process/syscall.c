
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

uintptr_t forkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    return 0;
}

uintptr_t exitSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->state = TERMINATED;
    return 0;
}

uintptr_t yieldSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL); // Only a process can be yielded
    Process* process = (Process*)frame;
    process->state = YIELDED;
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
    }
}

