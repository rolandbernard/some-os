
#include "kernel/init.h"

#include "error/log.h"
#include "process/process.h"

Error initAllSystems() {
    CHECKED(initLogSystem());
    CHECKED(initProcessSystem());
    return simpleError(SUCCESS);
}

