#ifndef _FILE_PROCESS_H_
#define _FILE_PROCESS_H_

#include "files/vfs/types.h"
#include "process/types.h"

void vfsFileDescriptorCopy(Process* process, VfsFileDescriptor* desc);

void vfsFileDescriptorClose(Process* process, VfsFileDescriptor* desc);

VfsFileDescriptor* getFileDescriptor(Process* process, int fd);

int putNewFileDescriptor(Process* process, int fd, int flags, VfsFile* file, bool replace);

void closeFileDescriptor(Process* process, int fd);

void closeAllProcessFiles(Process* process);

void closeExecProcessFiles(Process* process);

void forkFileDescriptors(Process* new_process, Process* old_process);

#endif
