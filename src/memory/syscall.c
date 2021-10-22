
#include <assert.h>

#include "memory/syscall.h"

#include "memory/pagealloc.h"
#include "memory/pagetable.h"
#include "memory/memspace.h"
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
        process->memory.brk = end;
        return old_brk;
    } else if (page_end > page_start) {
        for (uintptr_t i = page_start; i < page_end; i += PAGE_SIZE) {
            mapPage(
                process->memory.mem, i, (uintptr_t)zero_page,
                PAGE_ENTRY_USER | PAGE_ENTRY_READ | PAGE_ENTRY_AD | PAGE_ENTRY_COPY, 0
            );
        }
        process->memory.brk = end;
        return old_brk;
    } else {
        for (uintptr_t i = page_end; i < page_start; i += PAGE_SIZE) {
            unmapAndFreePage(process->memory.mem, i);
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

typedef struct {
    uintptr_t start;
    uintptr_t end;
    uintptr_t protect;
} ProtectSyscallRequest;

static void allPagesProtectCallback(PageTableEntry* entry, uintptr_t vaddr, void* udata) {
    ProtectSyscallRequest* request = (ProtectSyscallRequest*)udata;
    if (vaddr >= request->start && vaddr < request->end && (entry->bits & PAGE_ENTRY_USER) != 0) {
        entry->bits &= ~PAGE_ENTRY_RWX;
        if ((request->protect & PROT_READ) != 0) {
            entry->bits |= PAGE_ENTRY_READ;
        }
        if ((request->protect & PROT_WRITE) != 0) {
            entry->bits |= PAGE_ENTRY_WRITE;
        }
        if ((request->protect & PROT_EXEC) != 0) {
            entry->bits |= PAGE_ENTRY_EXEC;
        }
    }
}

void protectSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    uintptr_t addr = process->frame.regs[REG_ARGUMENT_1];
    uintptr_t length = process->frame.regs[REG_ARGUMENT_2];
    uintptr_t protect = process->frame.regs[REG_ARGUMENT_3];
    if ((protect & PROT_READ_WRITE_EXEC) == 0) {
        process->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    } else {
        if (length != 0) {
            ProtectSyscallRequest request = {
                .start = addr & -PAGE_SIZE,
                .end = (addr + length + PAGE_SIZE - 1) & -PAGE_SIZE,
                .protect = protect,
            };
            allPagesDo(process->memory.mem, allPagesProtectCallback, &request);
        }
        process->frame.regs[REG_ARGUMENT_0] = 0;
    }
}

