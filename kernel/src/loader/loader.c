
#include <string.h>
#include <assert.h>

#include "loader/loader.h"

#include "files/process.h"
#include "files/syscall.h"
#include "files/vfs/fs.h"
#include "files/vfs/file.h"
#include "loader/elf.h"
#include "memory/kalloc.h"
#include "memory/pagetable.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "process/process.h"
#include "task/schedule.h"
#include "task/task.h"
#include "task/types.h"
#include "util/util.h"

static size_t stringArrayLength(VirtPtr addr) {
    size_t length = 0;
    while (readInt(addr, sizeof(uintptr_t) * 8) != 0) {
        addr.address += sizeof(uintptr_t);
        length++;
    }
    return length;
}

static uintptr_t pushString(VirtPtr stack_pointer, VirtPtr string) {
    size_t length = strlenVirtPtr(string);
    stack_pointer.address -= length + 1;
    memcpyBetweenVirtPtr(stack_pointer, string, length + 1);
    return stack_pointer.address;
}

static uintptr_t pushStringArray(VirtPtr stack_pointer, VirtPtr array, size_t* size) {
    size_t length = stringArrayLength(array);
    uintptr_t strings[length];
    for (size_t i = length; i > 0;) {
        i--;
        stack_pointer.address = pushString(
            stack_pointer,
            virtPtrFor(readIntAt(array, i, sizeof(uintptr_t) * 8), stack_pointer.table)
        );
        strings[length] = stack_pointer.address;
    }
    stack_pointer.address -= sizeof(uintptr_t);
    writeInt(stack_pointer, sizeof(uintptr_t) * 8, 0);
    for (size_t i = length; i > 0;) {
        i--;
        stack_pointer.address -= sizeof(uintptr_t);
        writeInt(stack_pointer, sizeof(uintptr_t) * 8, strings[i]);
    }
    if (size != NULL) {
        *size = length;
    }
    return stack_pointer.address;
}

static void allPagesBrkCallback(PageTableEntry* entry, uintptr_t vaddr, void* udata) {
    uintptr_t* last_page = (uintptr_t*)udata;
    if (vaddr > *last_page) {
        *last_page = vaddr;
    }
}

static uintptr_t findStartBrk(MemorySpace* memspc) {
    uintptr_t last_page = 0;
    allPagesDo(memspc, allPagesBrkCallback, &last_page);
    return last_page + PAGE_SIZE;
}

Error loadProgramInto(Task* task, const char* path, VirtPtr args, VirtPtr envs) {
    VfsFile* file;
    CHECKED(vfsOpenAt(&global_file_system, task->process, NULL, path, VFS_OPEN_EXECUTE, 0, &file));
    VfsStat stat;
    CHECKED(vfsFileStat(file, task->process, virtPtrForKernel(&stat)), vfsFileClose(file));
    MemorySpace* memory = createMemorySpace();
    uintptr_t entry;
    CHECKED(loadProgramFromElfFile(memory, file, &entry), {
        deallocMemorySpace(memory);
        vfsFileClose(file);
    });
    vfsFileClose(file);
    // Find the start_brk in the memory
    uintptr_t start_brk = findStartBrk(memory);
    // Allocate stack
    if (!allocatePages(memory, USER_STACK_TOP - USER_STACK_SIZE, 0, USER_STACK_SIZE, ELF_PROG_READ | ELF_PROG_WRITE)) {
        deallocMemorySpace(memory);
        return simpleError(ENOMEM);
    }
    // Push arguments and env to stack
    uintptr_t envs_addr = pushStringArray(virtPtrFor(USER_STACK_TOP, memory), envs, NULL);
    size_t argc = 0;
    uintptr_t args_addr = pushStringArray(virtPtrFor(envs_addr, memory), args, &argc);
    // If everything went well, replace the original process
    if (task->process == NULL) {
        task->process = createUserProcess(NULL);
    } else {
        terminateAllProcessTasksBut(task->process, task);
    }
    deallocMemorySpace(task->process->memory.mem);
    if (stat.mode & VFS_MODE_SETUID) {
        task->process->resources.uid = stat.uid;
    }
    if (stat.mode & VFS_MODE_SETGID) {
        task->process->resources.gid = stat.gid;
    }
    task->process->memory.mem = memory;
    task->process->memory.start_brk = start_brk;
    task->process->memory.brk = start_brk;
    initTrapFrame(&task->frame, args_addr, 0, entry, task->process->pid, memory);
    // Set main function arguments
    task->frame.regs[REG_ARGUMENT_0] = argc;
    task->frame.regs[REG_ARGUMENT_1] = args_addr;
    task->frame.regs[REG_ARGUMENT_2] = envs_addr;
    // Close files with CLOEXEC flag
    closeExecProcessFiles(task->process);
    return simpleError(SUCCESS);
}

SyscallReturn execveSyscall(TrapFrame* frame) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyStringFromSyscallArgs(task, SYSCALL_ARG(0));
    if (string != NULL) {
        moveTaskToState(task, WAITING);
        Error e = loadProgramInto(task, string, virtPtrForTask(SYSCALL_ARG(1), task), virtPtrForTask(SYSCALL_ARG(2), task));
        dealloc(string);
        if (isError(e)) {
            SYSCALL_RETURN(-e.kind);
        } else {
            return CONTINUE;
        }
    } else {
        SYSCALL_RETURN(-EINVAL);
    }
}

