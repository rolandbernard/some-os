
#include "schedule/schedule.h"
#include "interrupt/trap.h"

#include "error/log.h"
void enqueueProcess(Process* process) {
    // TODO
    if (process->state == READY) {
        enterProcessAsUser(process);
    } else {
        for (;;) {
            waitForInterrupt();
        }
    }
}

