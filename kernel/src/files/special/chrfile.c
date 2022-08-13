
#include <string.h>

#include "files/special/chrfile.h"

#include "devices/devfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "process/types.h"
#include "task/syscall.h"

static Error serialReadFunction(SerialDeviceFile* file, Process* process, VirtPtr buffer, size_t size, size_t* read) {
    Error status;
    size_t i;
    lockSpinLock(&file->lock);
    for (i = 0; i < size; i++) {
        char c;
        status = file->serial.read(file->serial.data, &c);
        if (status.kind != SUCCESS) {
            break;
        }
        writeIntAt(buffer, 8, i, c);
    }
    unlockSpinLock(&file->lock);
    *read = i;
    if (status.kind == EAGAIN && i != 0) {
        return simpleError(SUCCESS);
    } else {
        // Success or any other error
        return status;
    }
}

static Error serialWriteFunction(SerialDeviceFile* file, Process* process, VirtPtr buffer, size_t size, size_t* written) {
    lockSpinLock(&file->lock);
    for (size_t i = 0; i < size; i++) {
        file->serial.write(file->serial.data, readIntAt(buffer, i, 8));
    }
    unlockSpinLock(&file->lock);
    *written = size;
    return simpleError(SUCCESS);
}

static Error serialStatFunction(SerialDeviceFile* file, Process* process, VirtPtr stat) {
    VfsStat ret = {
        .id = file->base.ino,
        .mode = TYPE_MODE(VFS_TYPE_CHAR) | VFS_MODE_OG_RW,
        .nlinks = 1,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .block_size = 0,
        .st_atime = getNanoseconds(),
        .st_mtime = getNanoseconds(),
        .st_ctime = getNanoseconds(),
        .dev = DEV_INO,
    };
    memcpyBetweenVirtPtr(stat, virtPtrForKernel(&ret), sizeof(VfsStat));
    return simpleError(SUCCESS);
}

static void serialFreeFunction(SerialDeviceFile* file) {
    dealloc(file);
}

static Error serialCopyFunction(SerialDeviceFile* file, Process* process, VfsFile** ret) {
    SerialDeviceFile* copy = kalloc(sizeof(SerialDeviceFile));
    lockSpinLock(&file->lock);
    memcpy(copy, file, sizeof(SerialDeviceFile));
    unlockSpinLock(&file->lock);
    initSpinLock(&copy->lock);
    *ret = (VfsFile*)copy;
    return simpleError(SUCCESS);
}

static const VfsFileVtable functions = {
    .read = (ReadFunction)serialReadFunction,
    .write = (WriteFunction)serialWriteFunction,
    .stat = (StatFunction)serialStatFunction,
    .free = (FileFreeFunction)serialFreeFunction,
    .copy = (CopyFunction)serialCopyFunction,
};

SerialDeviceFile* createSerialDeviceFile(size_t ino, Serial serial) {
    SerialDeviceFile* file = zalloc(sizeof(SerialDeviceFile));
    if (file != NULL) {
        file->base.functions = &functions;
        file->base.ino = ino;
        file->serial = serial;
    }
    return file;
}

