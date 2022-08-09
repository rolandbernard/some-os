
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

SyscallReturn timesSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    TimesStruct times;
    times.user_time = task->times.user_time;
    times.system_time = task->times.system_time;
    times.user_child_time = task->times.user_child_time;
    times.system_child_time = task->times.system_child_time;
    if (SYSCALL_ARG(0) != 0) {
        VirtPtr buf = virtPtrForTask(SYSCALL_ARG(0), task);
        memcpyBetweenVirtPtr(buf, virtPtrForKernel(&times), sizeof(TimesStruct));
    }
    SYSCALL_RETURN(getTime());
}

