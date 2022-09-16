#ifndef _VFS_FILE_H_
#define _VFS_FILE_H_

#include "files/vfs/types.h"
#include "process/types.h"

Error vfsFileSeek(VfsFile* file, Process* process, size_t offset, VfsSeekWhence whence, size_t* new_pos);

Error vfsFileRead(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* read);

Error vfsFileWrite(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* written);

Error vfsFileReadAt(VfsFile* file, Process* process, VirtPtr buffer, size_t offset, size_t length, size_t* read);

Error vfsFileWriteAt(VfsFile* file, Process* process, VirtPtr buffer, size_t offset, size_t length, size_t* written);

Error vfsFileStat(VfsFile* file, Process* process, VirtPtr ret);

Error vfsFileTrunc(VfsFile* file, Process* process, size_t size);

Error vfsFileChmod(VfsFile* file, Process* process, VfsMode mode);

Error vfsFileChown(VfsFile* file, Process* process, Uid uid, Gid gid);

Error vfsFileReaddir(VfsFile* file, Process* process, VirtPtr buffer, size_t length, size_t* read);

Error vfsFileIoctl(VfsFile* file, Process* process, size_t request, VirtPtr argp, uintptr_t* out);

bool vfsFileWillBlock(VfsFile* file, Process* process, bool write);

void vfsFileCopy(VfsFile* file);

void vfsFileClose(VfsFile* file);

#endif
