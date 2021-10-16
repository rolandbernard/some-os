
#include <string.h>

#include "devices/devfs.h"

#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "devices/devices.h"
#include "files/blkfile.h"
#include "files/chrfile.h"
#include "files/vfs.h"
#include "memory/kalloc.h"
#include "util/util.h"

static size_t countAllDeviceFiles() {
    size_t count = 0;
    count++; // For the tty0
    count += getDeviceCountOfType(VIRTIO_BLOCK);
    return count;
}

static void deviceSeekFunction(DeviceDirectoryFile* file, Uid uid, Gid gid, size_t offset, VfsSeekWhence whence, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->lock);
    size_t new_position;
    switch (whence) {
        case VFS_SEEK_CUR:
            new_position = file->entry + offset;
            break;
        case VFS_SEEK_SET:
            new_position = offset;
            break;
        case VFS_SEEK_END:
            new_position = countAllDeviceFiles() + offset;
            break;
    }
    file->entry = new_position;
    unlockSpinLock(&file->lock);
    callback(simpleError(SUCCESS), file->entry, udata);
}

static void deviceStatFunction(DeviceDirectoryFile* file, Uid uid, Gid gid, VfsFunctionCallbackStat callback, void* udata) {
    lockSpinLock(&file->lock);
    VfsStat ret = {
        // TODO: use real values
        .id = 0,
        .mode = TYPE_MODE(VFS_TYPE_DIR) | VFS_MODE_O_R | VFS_MODE_G_R | VFS_MODE_A_R,
        .nlinks = 0,
        .uid = 0,
        .gid = 0,
        .size = countAllDeviceFiles(),
        .block_size = 1,
        .st_atime = 0,
        .st_mtime = 0,
        .st_ctime = 0,
    };
    unlockSpinLock(&file->lock);
    callback(simpleError(SUCCESS), ret, udata);
}

static void deviceCloseFunction(
    DeviceDirectoryFile* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata
) {
    lockSpinLock(&file->lock);
    dealloc(file);
    callback(simpleError(SUCCESS), udata);
}

static void deviceDupFunction(DeviceDirectoryFile* file, Uid uid, Gid gid, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&file->lock);
    DeviceDirectoryFile* copy = kalloc(sizeof(DeviceDirectoryFile));
    memcpy(copy, file, sizeof(DeviceDirectoryFile));
    unlockSpinLock(&file->lock);
    unlockSpinLock(&copy->lock); // Also unlock the new file
    callback(simpleError(SUCCESS), (VfsFile*)copy, udata);
}

static size_t writeDirectoryEntryNamed(char* name, VfsFileType type, size_t off, VirtPtr buff, size_t size) {
    size_t name_len = strlen(name);
    size_t entry_size = name_len + 1 + sizeof(VfsDirectoryEntry);
    VfsDirectoryEntry* entry = kalloc(entry_size);
    entry->id = 0;
    entry->off = off;
    entry->len = entry_size;
    entry->type = type;
    memcpy(entry->name, name, name_len + 1);
    memcpyBetweenVirtPtr(buff, virtPtrForKernel(entry), umin(entry_size, size));
    return umin(entry_size, size);
}

static void deviceReaddirFunction(DeviceDirectoryFile* file, Uid uid, Gid gid, VirtPtr buff, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->lock);
    size_t written = 0;
    size_t position = file->entry;
    if (size != 0) {
        if (written == 0 && position == 0) {
            written = writeDirectoryEntryNamed("tty0", VFS_TYPE_CHAR, file->entry, buff, size);
        }
        position--;
        size_t num_block = getDeviceCountOfType(VIRTIO_BLOCK);
        if (written == 0 && position < num_block) {
            FORMAT_STRINGX(name, "blk%li", position);
            written = writeDirectoryEntryNamed(name, VFS_TYPE_BLOCK, file->entry, buff, size);
        }
        position -= num_block;
    }
    file->entry++;
    unlockSpinLock(&file->lock);
    callback(simpleError(SUCCESS), written, udata);
}

static const VfsFileVtable file_functions = {
    .seek = (SeekFunction)deviceSeekFunction,
    .stat = (StatFunction)deviceStatFunction,
    .close = (CloseFunction)deviceCloseFunction,
    .dup = (DupFunction)deviceDupFunction,
    .readdir = (ReaddirFunction)deviceReaddirFunction,
};

DeviceDirectoryFile* createDeviceDirectoryFile() {
    DeviceDirectoryFile* file = zalloc(sizeof(DeviceDirectoryFile));
    file->base.functions = &file_functions;
    return file;
}

static void deviceOpenFunction(
    DeviceFilesystem* fs, Uid uid, Gid gid, const char* path, VfsOpenFlags flags, VfsMode mode,
    VfsFunctionCallbackFile callback, void* udata
) {
    if (strcmp(path, "/") == 0) {
        VfsFile* file = (VfsFile*)createDeviceDirectoryFile();
        callback(simpleError(SUCCESS), file, udata);
        return;
    }
    if (strcmp(path, "/tty0") == 0) {
        VfsFile* file = (VfsFile*)createSerialDeviceFile(getDefaultSerialDevice());
        callback(simpleError(SUCCESS), file, udata);
        return;
    }
    if (strncmp(path, "/blk", 4) == 0 && path[4] != 0) {
        size_t id = 0;
        for (int i = 4; path[i] != 0; i++) {
            if (path[i] >= '0' && path[i] <= '9') {
                id *= 10;
                id += path[i] - '0';
            } else {
                id = -1;
                break;
            }
        }
        if (id >= 0) {
            if (id < getDeviceCountOfType(VIRTIO_BLOCK)) {
                VirtIOBlockDevice* device = (VirtIOBlockDevice*)getDeviceOfType(VIRTIO_BLOCK, id);
                if (device != NULL) {
                    VirtIOBlockDeviceLayout* info = (VirtIOBlockDeviceLayout*)device->virtio.mmio;
                    VfsFile* file = (VfsFile*)createBlockDeviceFile(
                        device, info->config.blk_size, info->config.capacity * info->config.blk_size,
                        (BlockOperationFunction)virtIOBlockDeviceOperation
                    );
                    callback(simpleError(SUCCESS), file, udata);
                    return;
                }
            }
        }
    }
    callback(simpleError(NO_SUCH_FILE), NULL, udata);
}

static void deviceFreeFunction(DeviceFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    dealloc(fs);
    callback(simpleError(SUCCESS), udata);
}

static void deviceInitFunction(DeviceFilesystem* fs, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    // Nothing to initialize yet
    callback(simpleError(SUCCESS), udata);
}

static const VfsFilesystemVtable fs_functions = {
    .open = (OpenFunction)deviceOpenFunction,
    .free = (FreeFunction)deviceFreeFunction,
    .init = (InitFunction)deviceInitFunction,
};

DeviceFilesystem* createDeviceFilesystem() {
    DeviceFilesystem* file = zalloc(sizeof(DeviceFilesystem));
    file->base.functions = &fs_functions;
    return file;
}

