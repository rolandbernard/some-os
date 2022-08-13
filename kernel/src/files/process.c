
#include "files/process.h"

#include "memory/kalloc.h"
#include "task/spinlock.h"

int allocateNewFileDescriptorId(Process* process) {
    int fd = process->resources.next_fd;
    process->resources.next_fd++;
    return fd;
}

FileDescriptor* getFileDescriptor(Process* process, int fd) {
    FileDescriptor* current = process->resources.files;
    while (current != NULL) {
        if (current->id == fd) {
            return current;
        } else {
            current = current->next;
        }
    }
    return NULL;
}

void putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file) {
    lockSpinLock(&file->ref_lock);
    file->ref_count++;
    unlockSpinLock(&file->ref_lock);
    FileDescriptor* desc = kalloc(sizeof(FileDescriptor));
    desc->id = fd;
    desc->flags = flags;
    desc->file = file;
    desc->next = process->resources.files;
    process->resources.files = desc;
}

static void removeFileDescriptor(FileDescriptor* desc) {
    VfsFile* file = desc->file;
    dealloc(desc);
    lockSpinLock(&file->ref_lock);
    file->ref_count--;
    if (file->ref_count == 0) {
        unlockSpinLock(&file->ref_lock);
        file->functions->free(file);
    } else {
        unlockSpinLock(&file->ref_lock);
    }
}

void closeFileDescriptor(Process* process, int fd) {
    FileDescriptor** current = &process->resources.files;
    while (*current != NULL) {
        if ((*current)->id == fd) {
            FileDescriptor* to_remove = *current;
            *current = to_remove->next;
            removeFileDescriptor(to_remove);
            break;
        } else {
            current = &(*current)->next;
        }
    }
}

void closeAllProcessFiles(Process* process) {
    FileDescriptor* current = process->resources.files;
    while (current != NULL) {
        FileDescriptor* to_remove = current;
        current = current->next;
        removeFileDescriptor(to_remove);
    }
    process->resources.files = NULL;
    process->resources.next_fd = 0;
}

void closeExecProcessFiles(Process* process) {
    FileDescriptor** current = &process->resources.files;
    while (current != NULL) {
        if (((*current)->flags & VFS_FILE_CLOEXEC) != 0) {
            FileDescriptor* to_remove = *current;
            *current = to_remove->next;
            removeFileDescriptor(to_remove);
        } else {
            current = &(*current)->next;
        }
    }
}

void forkFileDescriptors(Process* new_process, Process* old_process) {
    VfsFile* files = task->process->resources.files;
    while (files != NULL) {
        VfsFile* copy = NULL;
        Error err = files->functions->dup(files, NULL, &copy);
        if (isError(err)) {
            deallocTask(new_task);
            deallocProcess(new_process);
            SYSCALL_RETURN(-err.kind);
        }
        copy->fd = files->fd;
        copy->flags = files->flags;
        copy->next = new_process->resources.files;
        new_process->resources.files = copy;
        files = files->next;
    }
}

