
#include "kernel/init.h"

#include "error/backtrace.h"
#include "error/log.h"
#include "interrupt/plic.h"
#include "interrupt/trap.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "process/process.h"
#include "task/harts.h"

Error initAllSystems() {
    CHECKED(initPageAllocator());
    CHECKED(initKernelVirtualMemory());
    initPrimaryHart();
    CHECKED(initPlic());
    return simpleError(SUCCESS);
}

void initHart(int hartid) {
    setupHartFrame(hartid);
    initTraps();
    KERNEL_SUCCESS("Initialized hart %i", hartid);
}

void initPrimaryHart() {
    setupHartFrame(0);
    KERNEL_SUCCESS("Initialized hart 0");
}

