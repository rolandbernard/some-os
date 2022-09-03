
#include <string.h>

#include "files/special/chrfile.h"

#include "devices/serial/ttyctl.h"
#include "files/vfs/node.h"
#include "memory/kalloc.h"

typedef struct {
    VfsNode base;
    CharDevice* device;
    Termios ctrl;
} VfsTtyNode;

static void ttyNodeFree(VfsTtyNode* node) {
    vfsNodeClose(node->base.real_node);
    dealloc(node);
}

static Error ttyNodeWriteAt(VfsTtyNode* node, VirtPtr buff, size_t offset, size_t length, size_t* written) {
    // TODO: Implement at least the following flags
    // * ONLCR
    // * OCRNL
    // * ONOCR
    // * ONLRET
    return node->device->functions->write(node->device, buff, length, written);
}

static Error ttyNodeReadAt(VfsTtyNode* node, VirtPtr buff, size_t offset, size_t length, size_t* read) {
    // TODO: Implement at least the following flags
    // * INLCR
    // * IGNCR
    // * ICRNL
    // * ISIG
    // * ICANON
    // * ECHO
    // * ECHOE
    // * ECHOK
    // * ECHONL
    // * VTIME
    // * VMIN
    // * VEOF
    // * VEOL
    // * VERASE
    // * VINTR
    // * VKILL
    // * VQUIT
    return node->device->functions->read(node->device, buff, length, read, true);
}

static Error ttyNodeIoctl(VfsTtyNode* node, size_t request, VirtPtr argp, int* res) {
    switch (request) {
        case TCGETS:
            return simpleError(ENOTSUP);
        case TCSETSF:
            node->device->functions->flush(node->device);
            // fall through
        case TCSETS:
        case TCSETSW:
            return simpleError(ENOTSUP);
        case TIOCGPGRP:
            return simpleError(ENOTSUP);
        case TIOCSPGRP:
            return simpleError(ENOTSUP);
        case TCXONC:
            return simpleError(ENOTSUP);
        case TCFLSH:
            node->device->functions->flush(node->device);
            return simpleError(SUCCESS);
        default:
            return simpleError(ENOTTY);
    }
}

static const VfsNodeFunctions funcs = {
    .free = (VfsNodeFreeFunction)ttyNodeFree,
    .read_at = (VfsNodeReadAtFunction)ttyNodeReadAt,
    .write_at = (VfsNodeWriteAtFunction)ttyNodeWriteAt,
    .ioctl = (VfsNodeIoctlFunction)ttyNodeIoctl,
};

VfsTtyNode* createTtyNode(CharDevice* device, VfsNode* real_node) {
    VfsTtyNode* node = kalloc(sizeof(VfsTtyNode));
    node->base.functions = &funcs;
    node->base.superblock = NULL;
    memcpy(&node->base.stat, &real_node->stat, sizeof(VfsStat));
    node->base.stat.rdev = device->base.id;
    node->base.stat.size = 0;
    node->base.stat.block_size = 0;
    node->base.stat.blocks = 0;
    node->base.real_node = real_node;
    node->base.ref_count = 1;
    initTaskLock(&node->base.lock);
    node->base.mounted = NULL;
    node->device = device;
    // TODO: Init to correct defaults
    memset(&node->ctrl, 0, sizeof(Termios));
    return node;
}

VfsFile* createCharDeviceFile(VfsNode* node, CharDevice* device, char* path) {
    VfsFile* file = kalloc(sizeof(VfsFile));
    file->node = (VfsNode*)createTtyNode(device, node);
    file->path = path;
    file->ref_count = 1;
    file->offset = 0;
    file->flags = 0;
    initTaskLock(&file->lock);
    return file;
}

