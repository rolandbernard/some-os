
#include <stdint.h>
#include <stdbool.h>

#include "devices/devices.h"
#include "error/log.h"
#include "error/panic.h"
#include "interrupt/com.h"
#include "interrupt/plic.h"
#include "interrupt/syscall.h"
#include "interrupt/timer.h"
#include "interrupt/trap.h"
#include "memory/memspace.h"
#include "memory/virtmem.h"
#include "process/process.h"
#include "process/schedule.h"
#include "process/signals.h"

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

void machineTrap(uintptr_t cause, uintptr_t pc, uintptr_t val, uintptr_t scratch) {
    bool interrupt = (uintptr_t)cause >> (sizeof(uintptr_t) * 8 - 1);
    int code = (uintptr_t)cause & 0xff;
    if (interrupt && (code == 0 || code == 1 || code == 3)) {
        handleMachineSoftwareInterrupt();
    }
    KERNEL_LOG("[!] Unhandled machine trap: %p %p %p %s", pc, val, scratch, getCauseString(interrupt, code));
    panic();
}

void kernelTrap(uintptr_t cause, uintptr_t pc, uintptr_t val, TrapFrame* frame) {
    bool interrupt = cause >> (sizeof(uintptr_t) * 8 - 1);
    int code = cause & 0xff;
    if (frame == NULL) {
        // Can't handle traps before the hart was initialized. (initBasicHart)
        KERNEL_LOG("[!] Unhandled trap: %p %p %p %s", pc, val, frame, getCauseString(interrupt, code));
        panic();
    } else {
        if (frame->hart != NULL) {
            Process* process = (Process*)frame;
            process->times.user_time += getTime() - process->times.entered;
            moveToSchedState(process, ENQUEUEABLE);
            process->times.entered = getTime();
        }
        if (interrupt) {
            frame->pc = pc;
            switch (code) {
                case 0: // Software interrupt U-mode
                case 1: // Software interrupt S-mode
                case 3: // Software interrupt M-mode
                    handleMachineSoftwareInterrupt();
                    break;
                case 4: // Timer interrupt U-mode
                case 5: // Timer interrupt S-mode
                case 7: // Timer interrupt M-mode
                    handleTimerInterrupt();
                    break;
                case 8: // External interrupt U-mode
                case 9: // External interrupt S-mode
                case 11: // External interrupt M-mode
                    handleExternalInterrupt();
                    break;
                default:
                    KERNEL_LOG("[!] Unhandled interrupt: %p %p %p %s", pc, val, frame, getCauseString(interrupt, code));
                    break;
            }
        } else {
            switch (code) {
                case 8: // Environment call from U-mode
                    runSyscall(frame, false);
                    break;
                case 9: // Environment call from S-mode
                case 11: // Environment call from M-mode
                    runSyscall(frame, true);
                    break;
                case 12: // Instruction page fault
                case 13: // Load page fault
                case 15: // Store/AMO page fault
                    if (frame->hart == NULL) {
                        if (!handlePageFault(kernel_page_table, val)) {
                            KERNEL_LOG("[!] Unhandled exception: %p %p %p %s", pc, val, frame, getCauseString(interrupt, code));
                            panic();
                        }
                    } else {
                        Process* process = (Process*)frame;
                        if (!handlePageFault(process->memory.mem, val)) {
                            KERNEL_LOG("[!] Segmentation fault: %i %p %p %p %s", process->pid, pc, val, frame, getCauseString(interrupt, code));
                            addSignalToProcess(process, SIGSEGV);
                        }
                    }
                    break;
                default:
                    if (frame->hart == NULL) {
                        KERNEL_LOG("[!] Unhandled exception: %p %p %p %s", pc, val, frame, getCauseString(interrupt, code));
                        panic();
                    } else {
                        Process* process = (Process*)frame;
                        KERNEL_LOG("[!] Segmentation fault: %i %p %p %p %s", process->pid, pc, val, frame, getCauseString(interrupt, code));
                        addSignalToProcess(process, SIGSEGV);
                    }
                    break;
            }
        }
        if (frame->hart != NULL) {
            // TODO: system time is not actually correct
            Process* process = (Process*)frame;
            process->times.system_time += getTime() - process->times.entered;
            enqueueProcess(process);
            runNextProcess();
        } else {
            // This is not called from a process, but from kernel init or interrupt handler
            enterKernelMode(frame);
        }
    }
}

