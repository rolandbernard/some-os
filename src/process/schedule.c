
#include "process/schedule.h"

#include "interrupt/trap.h"
#include "error/log.h"

void enqueueProcess(Process* process) {
    // TODO
    if (process->state == READY) {
        enterProcess(process);
    } else {
        for (;;) {
            waitForInterrupt();
        }
    }
}

