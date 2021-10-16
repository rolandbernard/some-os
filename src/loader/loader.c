
#include <string.h>

#include "loader/loader.h"

#include "files/syscall.h"
#include "files/vfs.h"
#include "loader/elf.h"
#include "memory/kalloc.h"
#include "memory/pagetable.h"
#include "memory/pagealloc.h"
#include "memory/virtmem.h"
#include "process/schedule.h"
#include "util/util.h"
#include <assert.h>

#define USER_STACK_TOP (1UL << 38)
#define USER_STACK_SIZE (1UL << 19)

typedef struct {
    Process* process;
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
    request->file->functions->close(request->file, 0, 0, noop, NULL);
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
            request->callback(simpleError(ALREADY_IN_USE), request->udata);
            dealloc(request);
            return;
        }
        // Push arguments and env to stack
        uintptr_t envs = pushStringArray(virtPtrFor(USER_STACK_TOP, request->memory), request->envs, NULL);
        size_t argc = 0;
        uintptr_t args = pushStringArray(virtPtrFor(envs, request->memory), request->args, &argc);
        // If everything went well, replace the original process
        if (request->process->pid == 0) {
            // Kernel process transforming into a user process
            dealloc(request->process->memory.stack);
            request->process->pid = allocateNewPid();
            request->process->status = 0;
        } else {
            deallocMemorySpace(request->memory);
            freePageTable(request->process->memory.mem);
        }
        if (request->file_stat.mode & VFS_MODE_SETUID) {
            request->process->resources.uid = request->file_stat.uid;
        }
        if (request->file_stat.mode & VFS_MODE_SETGID) {
            request->process->resources.gid = request->file_stat.gid;
        }
        request->process->memory.stack = NULL;
        request->process->memory.mem = request->memory;
        request->process->memory.start_brk = start_brk;
        request->process->memory.brk = start_brk;
        initTrapFrame(&request->process->frame, args, 0, entry, request->process->pid, request->memory);
        // Set main function arguments
        request->process->frame.regs[REG_ARGUMENT_0] = argc;
        request->process->frame.regs[REG_ARGUMENT_1] = args;
        request->process->frame.regs[REG_ARGUMENT_2] = envs;
        // Close files with CLOEXEC flag
        for (size_t i = 0; i < request->process->resources.fd_count;) {
            if ((request->process->resources.filedes[i]->flags & VFS_FILE_CLOEXEC) != 0) {
                request->process->resources.filedes[i]->functions->close(
                    request->process->resources.filedes[i], 0, 0, noop, NULL
                );
                memmove(
                    request->process->resources.filedes + i,
                    request->process->resources.filedes + i + 1,
                    (request->process->resources.fd_count - i - 1) * sizeof(VfsFile*)
                );
                request->process->resources.fd_count--;
            } else {
                i++;
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
        request->file->functions->close(request->file, 0, 0, noop, NULL);
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
        file->functions->stat(file, 0, 0, fileStatCallback, request);
    }
}

void loadProgramInto(Process* process, const char* path, VirtPtr args, VirtPtr envs, ProgramLoadCallback callback, void* udata) {
    LoadProgramRequest* request = kalloc(sizeof(LoadProgramRequest));
    request->process = process;
    request->args = args;
    request->envs = envs;
    request->callback = callback;
    request->udata = udata;
    vfsOpen(&global_file_system, process->resources.uid, process->resources.gid, path, VFS_OPEN_EXECUTE, 0, openFileCallback, request);
}

static void loadProgramCallback(Error error, void* udata) {
    Process* process = (Process*)udata;
    if (isError(error)) {
        process->frame.regs[REG_ARGUMENT_0] = -error.kind;
    } // Otherwise the arguments have been set already
    moveToSchedState(process, ENQUEUEABLE);
    enqueueProcess(process);
}

void execveSyscall(bool is_kernel, TrapFrame* frame, SyscallArgs args) {
    assert(frame->hart != NULL);
    Process* process = (Process*)frame;
    char* string = copyPathFromSyscallArgs(process, args[0]);
    if (string != NULL) {
        moveToSchedState(process, WAITING);
        loadProgramInto(
            process, string, virtPtrFor(args[1], process->memory.mem),
            virtPtrFor(args[2], process->memory.mem), loadProgramCallback, process
        );
        dealloc(string);
    } else {
        process->frame.regs[REG_ARGUMENT_0] = -ILLEGAL_ARGUMENTS;
    }
}

