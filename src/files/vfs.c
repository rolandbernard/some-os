
#include <string.h>

#include "files/vfs.h"

#include "files/path.h"
#include "memory/kalloc.h"

VirtualFilesystem global_file_system;

Error initVirtualFileSystem() {
    return simpleError(SUCCESS);
}

VirtualFilesystem* createVirtualFilesystem();

void freeVirtualFilesystem(VirtualFilesystem* fs);

Error mountFilesystem(VirtualFilesystem* fs, VfsFilesystem* filesystem, const char* path);

Error mountFile(VirtualFilesystem* fs, VfsFile* file, const char* path);

Error mountRedirect(VirtualFilesystem* fs, const char* from, const char* to);

Error umount(VirtualFilesystem* fs, const char* from);

void vfsOpen(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFunctionCallbackFile callback, void* udata);

void vfsUnlink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* path, VfsFunctionCallbackVoid callback, void* udata);

void vfsLink(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

void vfsRename(VirtualFilesystem* fs, Uid uid, Gid gid, const char* old, const char* new, VfsFunctionCallbackVoid callback, void* udata);

