
#include "files/process.h"

#include "memory/kalloc.h"
#include "task/spinlock.h"

int allocateNewFileDescriptorId(Process* process) {
    lockSpinLock(&process->resources.lock);
    int fd = process->resources.next_fd;
    process->resources.next_fd++;
    unlockSpinLock(&process->resources.lock);
    return fd;
}

FileDescriptor* getFileDescriptor(Process* process, int fd) {
    lockSpinLock(&process->resources.lock);
    FileDescriptor* current = process->resources.files;
    while (current != NULL) {
        if (current->id == fd) {
            unlockSpinLock(&process->resources.lock);
            return current;
        } else {
            current = current->next;
        }
    }
    unlockSpinLock(&process->resources.lock);
    return NULL;
}

void putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file) {
    lockSpinLock(&process->resources.lock);
    lockSpinLock(&file->ref_lock);
    file->ref_count++;
    unlockSpinLock(&file->ref_lock);
    FileDescriptor* desc = kalloc(sizeof(FileDescriptor));
    desc->id = fd;
    desc->flags = flags;
    desc->file = file;
    desc->next = process->resources.files;
    process->resources.files = desc;
    unlockSpinLock(&process->resources.lock);
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
    lockSpinLock(&process->resources.lock);
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
    unlockSpinLock(&process->resources.lock);
}

void closeAllProcessFiles(Process* process) {
    lockSpinLock(&process->resources.lock);
    FileDescriptor* current = process->resources.files;
    while (current != NULL) {
        FileDescriptor* to_remove = current;
        current = current->next;
        removeFileDescriptor(to_remove);
    }
    process->resources.files = NULL;
    process->resources.next_fd = 0;
    unlockSpinLock(&process->resources.lock);
}

void closeExecProcessFiles(Process* process) {
    lockSpinLock(&process->resources.lock);
    FileDescriptor** current = &process->resources.files;
    while (*current != NULL) {
        if (((*current)->flags & VFS_FILE_CLOEXEC) != 0) {
            FileDescriptor* to_remove = *current;
            *current = to_remove->next;
            removeFileDescriptor(to_remove);
        } else {
            current = &(*current)->next;
        }
    }
    unlockSpinLock(&process->resources.lock);
}

void forkFileDescriptors(Process* new_process, Process* old_process) {
    lockSpinLock(&old_process->resources.lock);
    FileDescriptor* current = old_process->resources.files;
    while (current != NULL) {
        putNewFileDescriptor(new_process, current->id, current->flags, current->file);
        current = current->next;
    }
    unlockSpinLock(&old_process->resources.lock);
}

