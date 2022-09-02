
#include <assert.h>

#include "files/process.h"

#include "files/vfs/file.h"
#include "memory/kalloc.h"
#include "task/spinlock.h"

void vfsFileDescriptorCopy(Process* process, VfsFileDescriptor* desc) {
    lockTaskLock(&process->resources.lock);
    desc->ref_count++;
    unlockTaskLock(&process->resources.lock);
}

void vfsFileDescriptorClose(Process* process, VfsFileDescriptor* desc) {
    lockTaskLock(&process->resources.lock);
    desc->ref_count--;
    if (desc->ref_count == 0) {
        unlockTaskLock(&process->resources.lock);
        VfsFile* file = desc->file;
        dealloc(desc);
        vfsFileClose(file);
    } else {
        unlockTaskLock(&process->resources.lock);
    }
}

VfsFileDescriptor* getFileDescriptor(Process* process, int fd) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor* current = process->resources.files;
    while (current != NULL && current->id < fd) {
        current = current->next;
    }
    if (current != NULL && current->id == fd) {
        vfsFileDescriptorCopy(process, current);
        unlockTaskLock(&process->resources.lock);
        return current;
    } else {
        unlockTaskLock(&process->resources.lock);
        return NULL;
    }
}

int putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file) {
    lockTaskLock(&process->resources.lock);
    vfsFileCopy(file);
    VfsFileDescriptor** current = &process->resources.files;
    if (fd < 0) {
        fd = 0;
        while (*current != NULL && (*current)->id == fd) {
            current = &(*current)->next;
            fd++;
        }
    } else {
        closeFileDescriptor(process, fd);
        while (*current != NULL && (*current)->id < fd) {
            current = &(*current)->next;
        }
        if (*current != NULL && (*current)->id == fd) {
            VfsFileDescriptor* to_remove = *current;
            *current = to_remove->next;
            vfsFileDescriptorClose(process, to_remove);
        }
    }
    assert(*current == NULL || (*current)->id > fd);
    VfsFileDescriptor* desc = kalloc(sizeof(VfsFileDescriptor));
    desc->id = fd;
    desc->flags = flags;
    desc->file = file;
    desc->next = *current;
    desc->ref_count = 1;
    *current = desc;
    unlockTaskLock(&process->resources.lock);
    return fd;
}

void closeFileDescriptor(Process* process, int fd) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor** current = &process->resources.files;
    while (*current != NULL && (*current)->id < fd) {
        current = &(*current)->next;
    }
    if (*current != NULL && (*current)->id == fd) {
        VfsFileDescriptor* to_remove = *current;
        *current = to_remove->next;
        vfsFileDescriptorClose(process, to_remove);
    }
    unlockTaskLock(&process->resources.lock);
}

void closeAllProcessFiles(Process* process) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor* current = process->resources.files;
    while (current != NULL) {
        VfsFileDescriptor* to_remove = current;
        current = current->next;
        vfsFileDescriptorClose(process, to_remove);
    }
    process->resources.files = NULL;
    unlockTaskLock(&process->resources.lock);
}

void closeExecProcessFiles(Process* process) {
    lockTaskLock(&process->resources.lock);
    VfsFileDescriptor** current = &process->resources.files;
    while (*current != NULL) {
        if (((*current)->flags & VFS_DESC_CLOEXEC) != 0) {
            VfsFileDescriptor* to_remove = *current;
            *current = to_remove->next;
            vfsFileDescriptorClose(process, to_remove);
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

