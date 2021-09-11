
#include <string.h>

#include "files/vfs.h"

#include "files/path.h"
#include "memory/kalloc.h"
#include "util/spinlock.h"

static SpinLock next_lock;
static size_t next_id = 1;
static VfsVirtualDirectory root; // Virtual file system root

Error initVirtualFileSystem() {
    return simpleError(SUCCESS);
}

Error insertVirtualNode(char* path, VfsNode* file) {
    inlineReducePath(path);
    
    
    dealloc(path);
    return simpleError(SUCCESS);
}

static void virtualDirectoryOpenFunction(VfsVirtualDirectory* dir, const char* name, VfsFunctionCallbackNode callback, void* udata) {
    lockSpinLock(&dir->lock);
    VfsNode* node = NULL;
    VfsDirectory* mount = dir->mount;
    for (size_t i = 0; i < dir->entry_count; i++) {
        if (strcmp(dir->entries[i].name, name) == 0) {
            node = dir->entries[i].node;
            break;
        }
    }
    unlockSpinLock(&dir->lock);
    if (node != NULL) {
        callback(simpleError(SUCCESS), node, udata);
    } else if (mount != NULL) {
        // Delegate to the mounted file system
        mount->functions->open(mount, name, callback, udata);
    } else {
        callback(simpleError(NO_SUCH_FILE), NULL, udata);
    }
}

static void virtualDirectoryUnlinkFunction(VfsVirtualDirectory* dir, const char* name, VfsFunctionCallbackVoid callback, void* udata) {
}

static VfsDirectoryVtable dirVtable = {
    .open = (OpenFunction)virtualDirectoryOpenFunction,
    .unlink = NULL,
    .readdir = NULL,
    .reset = NULL,
    .createdir = NULL,
    .createfile = NULL,
    .stat = NULL,
    .close = NULL,
    .delete = NULL,
};

VfsVirtualDirectory* createVirtualDirectory() {
    VfsVirtualDirectory* dir = zalloc(sizeof(VfsVirtualDirectory));
    dir->base.functions = &dirVtable;
    return dir;
}

static VfsFileVtable fileVtable = {
    .tell = NULL,
    .seek = NULL,
    .read = NULL,
    .write = NULL,
    .close = NULL,
    .delete = NULL,
    .stat = NULL,
};

VfsVirtualFile* createVirtualFile() {
    VfsVirtualFile* file = zalloc(sizeof(VfsVirtualFile));
    file->base.functions = &fileVtable;
    lockSpinLock(&next_lock);
    file->stats.id = next_id;
    next_id++;
    unlockSpinLock(&next_lock);
    return file;
}

