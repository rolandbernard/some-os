
#include "kernel/init.h"

#include "error/log.h"
#include "interrupt/plic.h"
#include "process/process.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "interrupt/trap.h"
#include "process/harts.h"
#include "process/syscall.h"

Error initAllSystems() {
    CHECKED(initPageAllocator());
    CHECKED(initKernelVirtualMemory());
    // kalloc is available from here on.
    initPrimaryHart();
    CHECKED(initPlic());
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

