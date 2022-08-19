#ifndef _VFS_FS_H_
#define _VFS_FS_H_

#include "files/vfs/types.h"
#include "process/types.h"

extern VirtualFilesystem global_file_system;

Error vfsOpenAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret);

Error vfsMknodAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsMode mode, DeviceId id);

Error vfsUnlinkAt(VirtualFilesystem* fs, Process* process, VfsFile* file, const char* path, VfsUnlinkFlags flags);

Error vfsLinkAt(VirtualFilesystem* fs, Process* process, VfsFile* old_file, const char* old, VfsFile* new_file, const char* new);

Error vfsMount(VirtualFilesystem* fs, Process* process, const char* path, VfsSuperblock* sb);

Error vfsUmount(VirtualFilesystem* fs, Process* process, const char* path);

Error vfsCreateSuperblock(VirtualFilesystem* fs, Process* process, const char* path, const char* type, VirtPtr data, VfsSuperblock** ret);

Error canAccess(VfsStat* stat, struct Process_s* process, VfsAccessFlags flags);

#endif
