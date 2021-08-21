
#include "kernel/init.h"

#include "error/log.h"
#include "process/process.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"

Error initAllSystems() {
    /* CHECKED(initPageAllocator()); */
    /* CHECKED(initKernelVirtualMemory()); */
    // kalloc is available from here on.
    CHECKED(initLogSystem());
    CHECKED(initProcessSystem());
    return simpleError(SUCCESS);
}

