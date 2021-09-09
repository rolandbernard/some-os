
#include "kernel/init.h"

#include "error/log.h"
#include "interrupt/plic.h"
#include "process/process.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "interrupt/trap.h"
#include "process/harts.h"
#include "process/syscall.h"

Error initBasicSystems() {
    CHECKED(initPageAllocator());
    CHECKED(initKernelVirtualMemory());
    // kalloc is available from here on.
    initPrimaryHart();
    return simpleError(SUCCESS);
}

Error initAllSystems() {
    CHECKED(initPlic()); // Not required for running a process. Timer interrupts work already.
    return simpleError(SUCCESS);
}

void initHart() {
    initTraps();
}

void initPrimaryHart() {
    setupHartFrame();
}

void initHarts() {
    // TODO:
}

