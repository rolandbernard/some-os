
#include <string.h>

#include "devices/devfs.h"

#include "devices/devices.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "files/special/blkfile.h"
#include "files/special/chrfile.h"
#include "files/vfs/cache.h"
#include "files/vfs/fs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "task/syscall.h"
#include "util/text.h"
#include "util/util.h"

static void deviceFsNodeFree(VfsNode* base) {
    dealloc(base);
}

static const VfsNodeFunctions file_functions = {
    // This is a device file. We don't support anything but freeing it.
    // When opening, the actual device will be accessed.
    .free = deviceFsNodeFree,
};

static VfsFileType vfsTypeForDevice(Device* device) {
    switch (device->type) {
        case DEVICE_BLOCK:
            return VFS_TYPE_BLOCK;
        case DEVICE_CHAR:
            return VFS_TYPE_CHAR;
        default:
            return VFS_TYPE_UNKNOWN;
    }
}

static VfsNode* createDeviceFsDevNode(DeviceFilesystem* sb, Device* device) {
    VfsNode* node = kalloc(sizeof(VfsNode));
    node->functions = &file_functions;
    node->superblock = (VfsSuperblock*)sb;
    node->stat.dev = sb->base.id;
    node->stat.id = device->id + 1;
    node->stat.mode = TYPE_MODE(vfsTypeForDevice(device)) | VFS_MODE_OG_RW;
    node->stat.nlinks = device->name_id == 0 ? 2 : 1;
    node->stat.uid = 0;
    node->stat.gid = 0;
    node->stat.rdev = device->id;
    node->stat.size = 0;
    node->stat.block_size = 0;
    node->stat.blocks = 0;
    Time time = getNanosecondsWithFallback();
    node->stat.atime = time;
    node->stat.mtime = time;
    node->stat.ctime = time;
    node->ref_count = 0;
    initTaskLock(&node->lock);
    node->mounted = NULL;
    node->real_node = node;
    return node;
}

static size_t writeDirectoryEntryNamed(size_t ino, const char* name, VfsFileType type, size_t off, VirtPtr buff, size_t size) {
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

static Error deviceFsDirReaddirAt(VfsNode* node, VirtPtr buff, size_t offset, size_t length, size_t* file_read, size_t* buff_written) {
    size_t written = 0;
    if (offset == 0) {
        written = writeDirectoryEntryNamed(1, ".", VFS_TYPE_DIR, offset, buff, length);
    } else if (offset == 1) {
        written = writeDirectoryEntryNamed(1, "..", VFS_TYPE_DIR, offset, buff, length);
    } else {
        bool fst = false;
        Device* dev = getNthDevice(offset - 2, &fst);
        if (dev != NULL) {
            if (fst) {
                written = writeDirectoryEntryNamed(dev->id + 1, dev->name, vfsTypeForDevice(dev), offset, buff, length);
            } else {
                FORMAT_STRINGX(entry_name, "%s%zu", dev->name, dev->name_id);
                written = writeDirectoryEntryNamed(dev->id + 1, entry_name, vfsTypeForDevice(dev), offset, buff, length);
            }
        }
    }
    *file_read = written != 0 ? 1 : 0;
    *buff_written = written;
    return simpleError(SUCCESS);
}

Error deviceFsDirLookup(VfsNode* node, const char* name, size_t* ret) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        *ret = 1;
        return simpleError(SUCCESS);
    } else {
        size_t name_len = strlen(name);
        Device* dev;
        if (name[name_len - 1] >= '0' && name[name_len - 1] <= '9') {
            while (name_len > 0 && name[name_len - 1] >= '0' && name[name_len - 1] <= '9') {
                name_len--;
            }
            char* name_prefix = kalloc(name_len + 1);
            memcpy(name_prefix, name, name_len);
            name_prefix[name_len] = 0;
            size_t name_id = 0;
            while (name[name_len] != 0) {
                name_id *= 10;
                name_id += name[name_len] - '0';
                name_len++;
            }
            dev = getDeviceNamed(name_prefix, name_id);
            dealloc(name_prefix);
        } else {
            dev = getDeviceNamed(name, 0);
        }
        if (dev != NULL) {
            *ret = dev->id + 1;
            return simpleError(SUCCESS);
        } else {
            return simpleError(ENOENT);
        }
    }
}

static const VfsNodeFunctions root_dir_functions = {
    .free = deviceFsNodeFree,
    .readdir_at = deviceFsDirReaddirAt,
    .lookup = deviceFsDirLookup,
};

static VfsNode* createDeviceFsRootDirNode(DeviceFilesystem* sb) {
    VfsNode* node = kalloc(sizeof(VfsNode));
    node->functions = &root_dir_functions;
    node->superblock = (VfsSuperblock*)sb;
    node->stat.dev = sb->base.id;
    node->stat.id = 1;
    node->stat.mode = TYPE_MODE(VFS_TYPE_DIR) | 0555;
    node->stat.nlinks = 2;
    node->stat.uid = 0;
    node->stat.gid = 0;
    node->stat.rdev = 0;
    node->stat.size = 0;
    node->stat.block_size = 0;
    node->stat.blocks = 0;
    Time time = getNanosecondsWithFallback();
    node->stat.atime = time;
    node->stat.mtime = time;
    node->stat.ctime = time;
    node->ref_count = 0;
    initTaskLock(&node->lock);
    node->mounted = NULL;
    node->real_node = node;
    return node;
}

static void deviceFsSuperFree(VfsSuperblock* sb) {
    dealloc(sb);
}

static Error deviceFsReadNode(VfsSuperblock* sb, size_t node_id, VfsNode** ret) {
    Device* dev = getDeviceWithId(node_id - 1);
    if (dev != NULL) {
        *ret = createDeviceFsDevNode((DeviceFilesystem*)sb, dev);
        return simpleError(SUCCESS);
    } else {
        return simpleError(ENODEV);
    }
}

static Error deviceFsWriteNode(VfsSuperblock* sb, VfsNode* node) {
    // This filesystem is not persistent
    return simpleError(SUCCESS);
}

static Error deviceFsNewNode(VfsSuperblock* sb, size_t* ret) {
    return simpleError(EROFS);
}

static Error deviceFsFreeNode(VfsSuperblock* sb, VfsNode* ret) {
    return simpleError(EROFS);
}

static const VfsSuperblockFunctions sb_functions = {
    .free = deviceFsSuperFree,
    .read_node = deviceFsReadNode,
    .write_node = deviceFsWriteNode,
    .new_node = deviceFsNewNode,
    .free_node = deviceFsFreeNode,
};

Error createDeviceSuperblock(VfsFile* file, VirtPtr data, VfsSuperblock** out) {
    DeviceFilesystem* sb = zalloc(sizeof(DeviceFilesystem));
    sb->base.id = 0;
    sb->base.ref_count = 1;
    sb->base.functions = &sb_functions;
    sb->base.root_node = createDeviceFsRootDirNode(sb);
    initTaskLock(&sb->base.lock);
    vfsCacheInit(&sb->base.nodes);
    *out = (VfsSuperblock*)sb;
    return simpleError(SUCCESS);
}

Error registerFsDriverDevfs() {
    VfsFilesystemDriver* driver = kalloc(sizeof(VfsFilesystemDriver));
    driver->name = "dev";
    driver->flags = VFS_DRIVER_FLAGS_NOFILE;
    driver->create_superblock = createDeviceSuperblock;
    vfsRegisterFilesystemDriver(driver);
    return simpleError(SUCCESS);
}

