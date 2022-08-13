#ifndef _FILE_PROCESS_H_
#define _FILE_PROCESS_H_

#include "files/vfs.h"
#include "process/types.h"

int allocateNewFileDescriptorId(Process* process);

FileDescriptor* getFileDescriptor(Process* process, int fd);

void putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file);

void closeFileDescriptor(Process* process, int fd);

void closeAllProcessFiles(Process* process);

void closeExecProcessFiles(Process* process);

void forkFileDescriptors(Process* new_process, Process* old_process);

#endif
