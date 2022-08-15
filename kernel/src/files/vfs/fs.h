#ifndef _VFS_FS_H_
#define _VFS_FS_H_

#include "files/vfs/types.h"
#include "process/types.h"

extern VirtualFilesystem global_file_system;

Error vfsOpen(VirtualFilesystem* fs, Process* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret);

Error vfsMknod(VirtualFilesystem* fs, Process* process, const char* path, VfsMode mode, DeviceId id);

Error vfsUnlink(VirtualFilesystem* fs, Process* process, const char* path);

Error vfsLink(VirtualFilesystem* fs, Process* process, const char* old, const char* new);

Error vfsRootMount(VirtualFilesystem* fs, const char* path, VfsSuperblock* sb);

Error vfsMount(VirtualFilesystem* fs, const char* path, VfsSuperblock* sb);

Error vfsUmount(VirtualFilesystem* fs, const char* path);

Error vfsCreateSuperblock(VirtualFilesystem* fs, const char* path, const char* type, VirtPtr data, VfsSuperblock** ret);

bool canAccess(VfsMode mode, Uid file_uid, Gid file_gid, struct Process_s* process, VfsAccessFlags flags);

#endif
