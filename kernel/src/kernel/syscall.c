
#include <assert.h>

#include "kernel/syscall.h"

#include "kernel/time.h"
#include "memory/virtptr.h"
#include "process/process.h"

typedef struct {
    size_t user_time;
    size_t system_time;
    size_t user_child_time;
    size_t system_child_time;
} TimesStruct;

#define TIMES_CLOCK_SCALING(TIME) (TIME / 10)

SyscallReturn timesSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    if (SYSCALL_ARG(0) != 0) {
        TimesStruct times;
        times.user_time = TIMES_CLOCK_SCALING(task->times.user_time);
        times.system_time = TIMES_CLOCK_SCALING(task->times.system_time);
        times.user_child_time = TIMES_CLOCK_SCALING(task->times.user_child_time);
        times.system_child_time = TIMES_CLOCK_SCALING(task->times.system_child_time);
        VirtPtr buf = virtPtrForTask(SYSCALL_ARG(0), task);
        memcpyBetweenVirtPtr(buf, virtPtrForKernel(&times), sizeof(TimesStruct));
    }
    SYSCALL_RETURN(TIMES_CLOCK_SCALING(getTime()));
}

SyscallReturn nanosecondsSyscall(TrapFrame* frame) {
    SYSCALL_RETURN(getNanoseconds());
}

