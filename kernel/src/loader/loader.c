
#include <string.h>
#include <assert.h>

#include "loader/loader.h"

#include "files/syscall.h"
#include "files/vfs.h"
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

#define USER_STACK_TOP (1UL << 38)
#define USER_STACK_SIZE (1UL << 19)

typedef struct {
    Task* task;
    VirtPtr args;
    VirtPtr envs;
    ProgramLoadCallback callback;
    void* udata;
    VfsFile* file;
    PageTable* memory;
    VfsStat file_stat;
} LoadProgramRequest;

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

static uintptr_t findStartBrk(PageTable* table) {
    uintptr_t last_page = 0;
    allPagesDo(table, allPagesBrkCallback, &last_page);
    return last_page + PAGE_SIZE;
}

static void readElfFileCallback(Error error, uintptr_t entry, void* udata) {
    LoadProgramRequest* request = (LoadProgramRequest*)udata;
    request->file->functions->close(request->file, NULL, noop, NULL);
    if (isError(error)) {
        freePageTable(request->memory);
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        // Find the start_brk in the memory
        uintptr_t start_brk = findStartBrk(request->memory);
        // Allocate stack
        if (!allocatePages(request->memory, USER_STACK_TOP - USER_STACK_SIZE, 0, USER_STACK_SIZE, ELF_PROG_READ | ELF_PROG_WRITE)) {
            deallocMemorySpace(request->memory);
            request->callback(simpleError(ENOMEM), request->udata);
            dealloc(request);
            return;
        }
        // Push arguments and env to stack
        uintptr_t envs = pushStringArray(virtPtrFor(USER_STACK_TOP, request->memory), request->envs, NULL);
        size_t argc = 0;
        uintptr_t args = pushStringArray(virtPtrFor(envs, request->memory), request->args, &argc);
        // If everything went well, replace the original process
        if (request->task->process == NULL) {
            request->task->process = createUserProcess(NULL);
        } else {
            terminateAllProcessTasksBut(request->task->process, request->task);
        }
        deallocMemorySpace(request->task->process->memory.mem);
        if (request->file_stat.mode & VFS_MODE_SETUID) {
            request->task->process->resources.uid = request->file_stat.uid;
        }
        if (request->file_stat.mode & VFS_MODE_SETGID) {
            request->task->process->resources.gid = request->file_stat.gid;
        }
        request->task->process->memory.mem = request->memory;
        request->task->process->memory.start_brk = start_brk;
        request->task->process->memory.brk = start_brk;
        initTrapFrame(&request->task->frame, args, 0, entry, request->task->process->pid, request->memory);
        // Set main function arguments
        request->task->frame.regs[REG_ARGUMENT_0] = argc;
        request->task->frame.regs[REG_ARGUMENT_1] = args;
        request->task->frame.regs[REG_ARGUMENT_2] = envs;
        // Close files with CLOEXEC flag
        VfsFile** current = &request->task->process->resources.files;
        while (*current != NULL) {
            if (((*current)->flags & VFS_FILE_CLOEXEC) != 0) {
                VfsFile* to_remove = *current;
                *current = to_remove->next;
                to_remove->functions->close(to_remove, NULL, noop, NULL);
            } else {
                current = &(*current)->next;
            }
        }
        // Return from loading
        request->callback(simpleError(SUCCESS), request->udata);
        dealloc(request);
    }
}

static void fileStatCallback(Error error, VfsStat stat, void* udata) {
    LoadProgramRequest* request = (LoadProgramRequest*)udata;
    if (isError(error)) {
        request->file->functions->close(request->file, NULL, noop, NULL);
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        request->file_stat = stat;
        request->memory = createPageTable();
        loadProgramFromElfFile(request->memory, request->file, readElfFileCallback, request);
    }
}

static void openFileCallback(Error error, VfsFile* file, void* udata) {
    LoadProgramRequest* request = (LoadProgramRequest*)udata;
    if (isError(error)) {
        request->callback(error, request->udata);
        dealloc(request);
    } else {
        request->file = file;
        file->functions->stat(file, NULL, fileStatCallback, request);
    }
}

void loadProgramInto(Task* task, const char* path, VirtPtr args, VirtPtr envs, ProgramLoadCallback callback, void* udata) {
    LoadProgramRequest* request = kalloc(sizeof(LoadProgramRequest));
    request->task = task;
    request->args = args;
    request->envs = envs;
    request->callback = callback;
    request->udata = udata;
    vfsOpen(&global_file_system, task->process, path, VFS_OPEN_EXECUTE, 0, openFileCallback, request);
}

static void loadProgramCallback(Error error, void* udata) {
    Task* task = (Task*)udata;
    if (isError(error)) {
        task->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } else {
        task->sched.state = ENQUABLE;
        enqueueTask(task);
    }
}

void execveSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Task* task = (Task*)frame;
    char* string = copyPathFromSyscallArgs(task, args[0]);
    if (string != NULL) {
        task->sched.state = WAITING;
        loadProgramInto(
            task, string, virtPtrForTask(args[1], task), virtPtrForTask(args[2], task),
            loadProgramCallback, task
        );
        dealloc(string);
    } else {
        task->frame.regs[REG_ARGUMENT_0] = -EINVAL;
    }
}

