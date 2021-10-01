
#include <assert.h>

#include "memory/syscall.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "util/util.h"

static uintptr_t changeProcessBreak(Process* process, intptr_t change) {
    uintptr_t old_brk = process->memory.brk;
    uintptr_t end = old_brk + smax(change, -old_brk);
    if (end < process->memory.start_brk) {
        end = process->memory.start_brk;
    }
    uintptr_t page_start = (old_brk + PAGE_SIZE - 1) & -PAGE_SIZE;
    uintptr_t page_end = (end + PAGE_SIZE - 1) & -PAGE_SIZE;
    if (page_start == page_end) {
        return old_brk;
    } else if (page_end > page_start) {
        for (uintptr_t i = page_start; i < page_end; i += PAGE_SIZE) {
            void* page = allocPage();
            if (page == NULL) {
                // Can't allocate more space
                // Free all new pages
                for (uintptr_t j = i - PAGE_SIZE; j >= page_start; j -= PAGE_SIZE) {
                    deallocPage((void*)virtToPhys(process->memory.table, j));
                    unmapPage(process->memory.table, j);
                }
                return -1;
            } else {
                mapPage(
                    process->memory.table, i, (uintptr_t)page,
                    PAGE_ENTRY_USER | PAGE_ENTRY_RW | PAGE_ENTRY_AD, 0
                );
            }
        }
        process->memory.brk = end;
        return old_brk;
    } else {
        for (uintptr_t i = page_end; i < page_start; i += PAGE_SIZE) {
            deallocPage((void*)virtToPhys(process->memory.table, i));
            unmapPage(process->memory.table, i);
        }
        process->memory.brk = end;
        return old_brk;
    }
}

void sbrkSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    process->frame.regs[REG_ARGUMENT_0] =
        changeProcessBreak(process, process->frame.regs[REG_ARGUMENT_1]);
}

