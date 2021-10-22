
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
    Process* process = (Process*)frame;
    TimesStruct times;
    times.user_time = process->times.user_time;
    times.system_time = process->times.system_time;
    times.user_child_time = process->times.user_child_time;
    times.system_child_time = process->times.system_child_time;
    if (args[0] != 0) {
        VirtPtr buf = virtPtrFor(args[0], process->memory.mem);
        memcpyBetweenVirtPtr(buf, virtPtrForKernel(&times), sizeof(TimesStruct));
    }
    process->frame.regs[REG_ARGUMENT_0] = getTime();
}

