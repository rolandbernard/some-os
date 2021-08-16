
#include <stdint.h>
#include <stdbool.h>

#include "error/log.h"
#include "devices/devices.h"

const char* getCauseString(bool interrupt, int code) {
    if (interrupt) {
        switch (code) {
            case 0: return "User software interrupt";
            case 1: return "Supervisor software interrupt";
            case 3: return "Machine software interrupt";
            case 4: return "User timer interrupt";
            case 5: return "Supervisor timer interrupt";
            case 7: return "Machine timer interrupt";
            case 8: return "User external interrupt";
            case 9: return "Supervisor external interrupt";
            case 11: return "Machine external interrupt";
            default: return "Unknown";
        }
    } else {
        switch (code) {
            case 0: return "Instruction address misaligned";
            case 1: return "Instruction access fault";
            case 2: return "Illegal instruction";
            case 3: return "Breakpoint";
            case 4: return "Load address misaligned";
            case 5: return "Load access fault";
            case 6: return "Store/AMO address misaligned";
            case 7: return "Store/AMO access fault";
            case 8: return "Environment call from U-mode";
            case 9: return "Environment call from S-mode";
            case 11: return "Environment call from M-mode";
            case 12: return "Instruction page fault";
            case 13: return "Load page fault";
            case 15: return "Store/AMO page fault";
            default: return "Unknown";
        }
    }
}

void kernelTrap(uint64_t cause, uint64_t pc, uint64_t val, uint64_t scratch) {
    bool interrupt = cause >> 63;
    int code = cause & 0xff;
    logKernelMessage("[+] Unhandled trap: %016x %016x %016x %s", pc, val, scratch, getCauseString(interrupt, code));
}

