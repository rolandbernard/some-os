
#include <string.h>

#include "devices/devfs.h"

#include "devices/devices.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "files/special/blkfile.h"
#include "files/special/chrfile.h"
#include "files/vfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "task/syscall.h"
#include "util/util.h"

static size_t countAllDeviceFiles() {
    size_t count = 0;
    count++; // For the tty0
    count += getDeviceCountOfType(VIRTIO_BLOCK);
    return count;
}

static Error deviceSeekFunction(DeviceDirectoryFile* file, Process* process, size_t offset, VfsSeekWhence whence, size_t* ret) {
    lockSpinLock(&file->lock);
    size_t new_position = 0;
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
    *ret = file->entry;
    unlockSpinLock(&file->lock);
    return simpleError(SUCCESS);
}

static Error deviceStatFunction(DeviceDirectoryFile* file, Process* process, VirtPtr stat) {
    lockSpinLock(&file->lock);
    VfsStat ret = {
        .id = 1,
        .mode = TYPE_MODE(VFS_TYPE_DIR) | VFS_MODE_O_R | VFS_MODE_G_R | VFS_MODE_A_R,
        .nlinks = 0,
        .uid = 0,
        .gid = 0,
        .size = countAllDeviceFiles(),
        .block_size = 1,
        .st_atime = getNanoseconds(),
        .st_mtime = getNanoseconds(),
        .st_ctime = getNanoseconds(),
        .dev = DEV_INO,
    };
    unlockSpinLock(&file->lock);
    memcpyBetweenVirtPtr(stat, virtPtrForKernel(&ret), sizeof(VfsStat));
    return simpleError(SUCCESS);
}

static void deviceCloseFunction(DeviceDirectoryFile* file) {
    TrapFrame* lock = criticalEnter();
    lockSpinLock(&file->lock);
    dealloc(file);
    criticalReturn(lock);
}

static Error deviceDupFunction(DeviceDirectoryFile* file, Process* process, VfsFile** ret) {
    TrapFrame* lock = criticalEnter();
    lockSpinLock(&file->lock);
    DeviceDirectoryFile* copy = kalloc(sizeof(DeviceDirectoryFile));
    memcpy(copy, file, sizeof(DeviceDirectoryFile));
    unlockSpinLock(&file->lock);
    unlockSpinLock(&copy->lock); // Also unlock the new file
    criticalReturn(lock);
    *ret = (VfsFile*)copy;
    return simpleError(SUCCESS);
}

static size_t writeDirectoryEntryNamed(size_t ino, char* name, VfsFileType type, size_t off, VirtPtr buff, size_t size) {
    size_t name_len = strlen(name);
    size_t entry_size = name_len + 1 + sizeof(VfsDirectoryEntry);
    VfsDirectoryEntry* entry = kalloc(entry_size);
    entry->id = ino;
    entry->off = off;
    entry->len = entry_size;
    entry->type = type;
    memcpy(entry->name, name, name_len + 1);
    memcpyBetweenVirtPtr(buff, virtPtrForKernel(entry), umin(entry_size, size));
    return umin(entry_size, size);
}

static Error deviceReaddirFunction(DeviceDirectoryFile* file, Process* process, VirtPtr buff, size_t size, size_t* ret) {
    lockSpinLock(&file->lock);
    size_t written = 0;
    size_t position = file->entry;
    size_t ino = 1;
    if (size != 0) {
        if (written == 0 && position == 0) {
            // inode 1
            written = writeDirectoryEntryNamed(ino, ".", VFS_TYPE_DIR, file->entry, buff, size);
        }
        ino++;
        position--;
        if (written == 0 && position == 0) {
            // inode 2
            written = writeDirectoryEntryNamed(ino, "..", VFS_TYPE_DIR, file->entry, buff, size);
        }
        ino++;
        position--;
        if (written == 0 && position == 0) {
            written = writeDirectoryEntryNamed(ino, "tty0", VFS_TYPE_CHAR, file->entry, buff, size);
        }
        ino++;
        position--;
        size_t num_block = getDeviceCountOfType(VIRTIO_BLOCK);
        if (written == 0 && position < num_block) {
            FORMAT_STRINGX(name, "blk%li", position);
            written = writeDirectoryEntryNamed(ino + position, name, VFS_TYPE_BLOCK, file->entry, buff, size);
        }
        ino += num_block;;
        position -= num_block;
    }
    file->entry++;
    unlockSpinLock(&file->lock);
    *ret = written;
    return simpleError(SUCCESS);
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

static Error deviceOpenFunction(
    DeviceFilesystem* fs, Process* process, const char* path, VfsOpenFlags flags, VfsMode mode, VfsFile** ret
) {
    size_t ino = 2; // 1 and 2 reserved for . and ..
    if (strcmp(path, "/") == 0) {
        *ret = (VfsFile*)createDeviceDirectoryFile();
        return simpleError(SUCCESS);
    }
    ino++;
    if (strcmp(path, "/tty0") == 0) {
        *ret = (VfsFile*)createSerialDeviceFile(ino, getDefaultSerialDevice());
        return simpleError(SUCCESS);
    }
    ino++;
    if (strncmp(path, "/blk", 4) == 0 && path[4] != 0) {
        int id = 0;
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
            if (id < (int)getDeviceCountOfType(VIRTIO_BLOCK)) {
                VirtIOBlockDevice* device = (VirtIOBlockDevice*)getDeviceOfType(VIRTIO_BLOCK, id);
                if (device != NULL) {
                    VirtIOBlockDeviceLayout* info = (VirtIOBlockDeviceLayout*)device->virtio.mmio;
                    *ret = (VfsFile*)createBlockDeviceFile(
                        ino + id, device, info->config.blk_size,
                        info->config.capacity * info->config.blk_size,
                        (BlockOperationFunction)virtIOBlockDeviceOperation
                    );
                    return simpleError(SUCCESS);
                }
            }
        }
    }
    ino += getDeviceCountOfType(VIRTIO_BLOCK);
    return simpleError(ENOENT);
}

static void deviceFreeFunction(DeviceFilesystem* fs) {
    dealloc(fs);
}

static Error deviceInitFunction(DeviceFilesystem* fs, Process* process) {
    // Nothing to initialize yet
    return simpleError(SUCCESS);
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

