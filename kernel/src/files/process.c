
#include "files/process.h"

#include "files/vfs/file.h"
#include "memory/kalloc.h"
#include "task/spinlock.h"

int allocateNewFileDescriptorId(Process* process) {
    lockTaskLock(&process->resources.lock);
    int fd = process->resources.next_fd;
    process->resources.next_fd++;
    unlockTaskLock(&process->resources.lock);
    return fd;
}

VfsFileDescriptor* getFileDescriptor(Process* process, int fd) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor* current = process->resources.files;
    while (current != NULL) {
        if (current->id == fd) {
            unlockTaskLock(&process->resources.lock);
            return current;
        } else {
            current = current->next;
        }
    }
    unlockTaskLock(&process->resources.lock);
    return NULL;
}

void putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file) {
    lockTaskLock(&process->resources.lock);
    vfsFileCopy(file);
    VfsFileDescriptor* desc = kalloc(sizeof(VfsFileDescriptor));
    desc->id = fd;
    desc->flags = flags;
    desc->file = file;
    desc->next = process->resources.files;
    process->resources.files = desc;
    unlockTaskLock(&process->resources.lock);
}

static void removeFileDescriptor(VfsFileDescriptor* desc) {
    VfsFile* file = desc->file;
    dealloc(desc);
    vfsFileClose(file);
}

void closeFileDescriptor(Process* process, int fd) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor** current = &process->resources.files;
    while (*current != NULL) {
        if ((*current)->id == fd) {
            VfsFileDescriptor* to_remove = *current;
            *current = to_remove->next;
            removeFileDescriptor(to_remove);
            break;
        } else {
            current = &(*current)->next;
        }
    }
    unlockTaskLock(&process->resources.lock);
}

void closeAllProcessFiles(Process* process) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor* current = process->resources.files;
    while (current != NULL) {
        VfsFileDescriptor* to_remove = current;
        current = current->next;
        removeFileDescriptor(to_remove);
    }
    process->resources.files = NULL;
    process->resources.next_fd = 0;
    unlockTaskLock(&process->resources.lock);
}

void closeExecProcessFiles(Process* process) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor** current = &process->resources.files;
    while (*current != NULL) {
        if (((*current)->flags & VFS_DESC_CLOEXEC) != 0) {
            VfsFileDescriptor* to_remove = *current;
            *current = to_remove->next;
            removeFileDescriptor(to_remove);
        } else {
            current = &(*current)->next;
        }
    }
    unlockTaskLock(&process->resources.lock);
}

void forkFileDescriptors(Process* new_process, Process* old_process) {
    lockTaskLock(&old_process->resources.lock);
    VfsFileDescriptor* current = old_process->resources.files;
    while (current != NULL) {
        putNewFileDescriptor(new_process, current->id, current->flags, current->file);
        current = current->next;
    }
    unlockTaskLock(&old_process->resources.lock);
}

