
#include <assert.h>
#include <string.h>

#include "files/special/blkfile.h"

#include "devices/devfs.h"
#include "error/error.h"
#include "error/log.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "memory/virtptr.h"
#include "task/schedule.h"
#include "task/syscall.h"
#include "util/util.h"

static Error blockSeekFunction(BlockDeviceFile* file, Process* process, size_t offset, VfsSeekWhence whence, size_t* new_pos) {
    lockSpinLock(&file->lock);
    size_t new_position = 0;
    switch (whence) {
        case VFS_SEEK_CUR:
            new_position = file->position + offset;
            break;
        case VFS_SEEK_SET:
            new_position = offset;
            break;
        case VFS_SEEK_END:
            new_position = file->size + offset;
            break;
    }
    file->position = new_position;
    unlockSpinLock(&file->lock);
    *new_pos = new_position;
    return simpleError(SUCCESS);
}

typedef struct {
    Error result;
    Task* wakeup;
} BlockFileWakeup;

static void blockOperatonCallback(Error status, BlockFileWakeup* wakeup) {
    wakeup->result = status;
    wakeup->wakeup->sched.state = ENQUABLE;
    enqueueTask(wakeup->wakeup);
}

static Error syncBlockOperation(BlockOperationFunction func, void* dev, VirtPtr buf, size_t off, size_t size, bool write) {
    Task* self = getCurrentTask();
    assert(self != NULL);
    BlockFileWakeup wakeup;
    wakeup.wakeup = self;
    TrapFrame* lock = criticalEnter();
    self->sched.state = WAITING;
    func(dev, buf, off, size, write, (BlockOperatonCallback)blockOperatonCallback, &wakeup);
    criticalReturn(lock);
    return wakeup.result;
}

static Error genericBlockFileFunction(BlockDeviceFile* file, bool write, VirtPtr buffer, size_t size, size_t offset, size_t* ret) {
    void* block_device = file->device;
    size_t block_size = file->block_size;
    BlockOperationFunction block_op = file->block_operation;
    char* tmp_buffer = kalloc(block_size);
    size_t left = size;
    while (left > 0) {
        size_t size_diff;
        if (offset % block_size == 0 && left > block_size) {
            size_diff = left & -block_size;
            CHECKED(syncBlockOperation(block_op, block_device, buffer, offset, size_diff, write), dealloc(tmp_buffer));
        } else {
            size_diff = umin(left, block_size - offset % block_size);
            size_t tmp_start = offset & -block_size;
            CHECKED(syncBlockOperation(block_op, block_device, virtPtrForKernel(tmp_buffer), tmp_start, block_size, false), dealloc(tmp_buffer));
            if (write) {
                memcpyBetweenVirtPtr(virtPtrForKernel(tmp_buffer + offset % block_size), buffer, size_diff);
                CHECKED(syncBlockOperation(block_op, block_device, virtPtrForKernel(tmp_buffer), tmp_start, block_size, true), dealloc(tmp_buffer));
            } else {
                memcpyBetweenVirtPtr(buffer, virtPtrForKernel(tmp_buffer + offset % block_size), size_diff);
            }
        }
        left -= size_diff;
        offset += size_diff;
        buffer.address += size_diff;
    }
    dealloc(tmp_buffer);
    *ret = size - left;
    return simpleError(SUCCESS);
}

static Error blockReadFunction(BlockDeviceFile* file, Process* process, VirtPtr buffer, size_t size, size_t* read) {
    lockSpinLock(&file->lock);
    size_t offset = file->position;
    file->position += size;
    unlockSpinLock(&file->lock);
    return genericBlockFileFunction(file, false, buffer, size, offset, read);
}

static Error blockWriteFunction(BlockDeviceFile* file, Process* process, VirtPtr buffer, size_t size, size_t* written) {
    lockSpinLock(&file->lock);
    size_t offset = file->position;
    file->position += size;
    unlockSpinLock(&file->lock);
    return genericBlockFileFunction(file, true, buffer, size, offset, written);
}

static Error blockReadAtFunction(BlockDeviceFile* file, Process* process, VirtPtr buffer, size_t size, size_t offset, size_t* read) {
    return genericBlockFileFunction(file, false, buffer, size, offset, read);
}

static Error blockWriteAtFunction(BlockDeviceFile* file, Process* process, VirtPtr buffer, size_t size, size_t offset, size_t* written) {
    return genericBlockFileFunction(file, true, buffer, size, offset, written);
}

static Error blockStatFunction(BlockDeviceFile* file, Process* process, VirtPtr stat) {
    VfsStat ret = {
        .id = file->base.ino,
        .mode = TYPE_MODE(VFS_TYPE_BLOCK) | VFS_MODE_OG_RW,
        .nlinks = 1,
        .uid = 0,
        .gid = 0,
        .size = file->block_size,
        .block_size = file->block_size,
        .st_atime = getNanoseconds(),
        .st_mtime = getNanoseconds(),
        .st_ctime = getNanoseconds(),
        .dev = DEV_INO,
    };
    memcpyBetweenVirtPtr(stat, virtPtrForKernel(&ret), sizeof(VfsStat));
    return simpleError(SUCCESS);
}

static void blockCloseFunction(BlockDeviceFile* file) {
    dealloc(file);
}

static Error blockDupFunction(BlockDeviceFile* file, Process* process, VfsFile** ret) {
    BlockDeviceFile* copy = kalloc(sizeof(BlockDeviceFile));
    lockSpinLock(&file->lock);
    memcpy(copy, file, sizeof(BlockDeviceFile));
    unlockSpinLock(&file->lock);
    initSpinLock(&copy->lock);
    *ret = (VfsFile*)copy;
    return simpleError(SUCCESS);
}

static const VfsFileVtable functions = {
    .seek = (SeekFunction)blockSeekFunction,
    .read = (ReadFunction)blockReadFunction,
    .write = (WriteFunction)blockWriteFunction,
    .read_at = (ReadAtFunction)blockReadAtFunction,
    .write_at = (WriteAtFunction)blockWriteAtFunction,
    .stat = (StatFunction)blockStatFunction,
    .close = (CloseFunction)blockCloseFunction,
    .dup = (DupFunction)blockDupFunction,
};

BlockDeviceFile* createBlockDeviceFile(size_t ino, void* block_dev, size_t block_size, size_t size, BlockOperationFunction block_op) {
    BlockDeviceFile* file = zalloc(sizeof(BlockDeviceFile));
    if (file != NULL) {
        file->base.functions = &functions;
        file->base.ino = ino;
        file->device = block_dev;
        file->size = size;
        file->block_size = block_size;
        file->block_operation = block_op;
    }
    return file;
}

