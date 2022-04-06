
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

void timesSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    TimesStruct times;
    times.user_time = task->times.user_time;
    times.system_time = task->times.system_time;
    times.user_child_time = task->times.user_child_time;
    times.system_child_time = task->times.system_child_time;
    if (args[0] != 0) {
        VirtPtr buf = virtPtrForTask(args[0], task);
        memcpyBetweenVirtPtr(buf, virtPtrForKernel(&times), sizeof(TimesStruct));
    }
    task->frame.regs[REG_ARGUMENT_0] = getTime();
}

