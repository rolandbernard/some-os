
#include <string.h>

#include "files/chrfile.h"

#include "kernel/time.h"
#include "memory/kalloc.h"
#include "devices/devfs.h"

static void serialReadFunction(SerialDeviceFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
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
    if (status.kind == NO_DATA && i != 0) {
        callback(simpleError(SUCCESS), i, udata);
    } else {
        // Success or any other error
        callback(status, i, udata);
    }
}

static void serialWriteFunction(SerialDeviceFile* file, Uid uid, Gid gid, VirtPtr buffer, size_t size, VfsFunctionCallbackSizeT callback, void* udata) {
    lockSpinLock(&file->lock);
    for (size_t i = 0; i < size; i++) {
        file->serial.write(file->serial.data, readIntAt(buffer, i, 8));
    }
    unlockSpinLock(&file->lock);
    callback(simpleError(SUCCESS), size, udata);
}

static void serialStatFunction(SerialDeviceFile* file, Uid uid, Gid gid, VfsFunctionCallbackStat callback, void* udata) {
    lockSpinLock(&file->lock);
    VfsStat ret = {
        .id = file->base.ino,
        .mode = TYPE_MODE(VFS_TYPE_CHAR) | VFS_MODE_OG_RW,
        .nlinks = 0,
        .uid = 0,
        .gid = 0,
        .size = 0,
        .block_size = 0,
        .st_atime = getNanoseconds(),
        .st_mtime = getNanoseconds(),
        .st_ctime = getNanoseconds(),
        .dev = DEV_INO,
    };
    unlockSpinLock(&file->lock);
    callback(simpleError(SUCCESS), ret, udata);
}

static void serialCloseFunction(SerialDeviceFile* file, Uid uid, Gid gid, VfsFunctionCallbackVoid callback, void* udata) {
    lockSpinLock(&file->lock);
    dealloc(file);
    callback(simpleError(SUCCESS), udata);
}

static void serialDupFunction(SerialDeviceFile* file, Uid uid, Gid gid, VfsFunctionCallbackFile callback, void* udata) {
    lockSpinLock(&file->lock);
    SerialDeviceFile* copy = kalloc(sizeof(SerialDeviceFile));
    memcpy(copy, file, sizeof(SerialDeviceFile));
    unlockSpinLock(&file->lock);
    unlockSpinLock(&copy->lock); // Also unlock the new file
    callback(simpleError(SUCCESS), (VfsFile*)copy, udata);
}

static const VfsFileVtable functions = {
    .read = (ReadFunction)serialReadFunction,
    .write = (WriteFunction)serialWriteFunction,
    .stat = (StatFunction)serialStatFunction,
    .close = (CloseFunction)serialCloseFunction,
    .dup = (DupFunction)serialDupFunction,
};

SerialDeviceFile* createSerialDeviceFile(size_t ino, Serial serial) {
    SerialDeviceFile* file = zalloc(sizeof(SerialDeviceFile));
    file->base.functions = &functions;
    file->base.ino = ino;
    file->serial = serial;
    return file;
}

