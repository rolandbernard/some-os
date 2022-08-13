
#include <string.h>

#include "files/special/fifo.h"

#include "files/vfs.h"
#include "kernel/time.h"
#include "memory/kalloc.h"
#include "util/stringmap.h"

static SpinLock fifo_name_lock;
static StringMap named_data = STRING_MAP_INITIALIZER;

static PipeSharedData* getDataForName(const char* name, const char** unique_name) {
    lockSpinLock(&fifo_name_lock);
    PipeSharedData* ret = getFromStringMap(&named_data, name);
    if (ret == NULL) {
        ret = createPipeSharedData();
        if (ret != NULL) {
            putToStringMap(&named_data, name, ret);
        }
    } else {
        lockSpinLock(&ret->lock);
        ret->ref_count++;
        unlockSpinLock(&ret->lock);
    }
    *unique_name = getKeyFromStringMap(&named_data, name);
    unlockSpinLock(&fifo_name_lock);
    return ret;
}

static void increaseReferenceFor(const char* name) {
    lockSpinLock(&fifo_name_lock);
    PipeSharedData* data = getFromStringMap(&named_data, name);
    if (data != NULL) {
        lockSpinLock(&data->lock);
        data->ref_count++;
        unlockSpinLock(&data->lock);
    }
    unlockSpinLock(&fifo_name_lock);
}

static void decreaseReferenceFor(const char* name) {
    lockSpinLock(&fifo_name_lock);
    PipeSharedData* data = getFromStringMap(&named_data, name);
    if (data != NULL) {
        lockSpinLock(&data->lock);
        data->ref_count--;
        if (data->ref_count == 0) {
            unlockSpinLock(&data->lock);
            dealloc(data);
            deleteFromStringMap(&named_data, name);
        } else {
            unlockSpinLock(&data->lock);
        }
    }
    unlockSpinLock(&fifo_name_lock);
}

static Error fifoReadFunction(FifoFile* file, Process* process, VirtPtr buffer, size_t size, size_t* ret) {
    if (canAccess(file->base.mode, file->base.uid, file->base.gid, process, VFS_ACCESS_R)) {
        return executePipeOperation(file->data, process, buffer, size, false, ret);
    } else {
        return simpleError(EACCES);
    }
}

static Error fifoWriteFunction(FifoFile* file, Process* process, VirtPtr buffer, size_t size, size_t* ret) {
    if (canAccess(file->base.mode, file->base.uid, file->base.gid, process, VFS_ACCESS_W)) {
        return executePipeOperation(file->data, process, buffer, size, true, ret);
    } else {
        return simpleError(EACCES);
    }
}

static Error fifoStatFunction(FifoFile* file, Process* process, VirtPtr stat) {
    VfsStat ret = {
        .id = file->base.ino,
        .mode = file->base.mode,
        .nlinks = file->data->ref_count,
        .uid = file->base.uid,
        .gid = file->base.gid,
        .size = file->data->count,
        .block_size = 0,
        .st_atime = getNanoseconds(),
        .st_mtime = getNanoseconds(),
        .st_ctime = getNanoseconds(),
        .dev = 0,
    };
    memcpyBetweenVirtPtr(stat, virtPtrForKernel(&ret), sizeof(VfsStat));
    return simpleError(SUCCESS);
}

static void fifoFreeFunction(FifoFile* file) {
    decreaseReferenceFor(file->name);
    dealloc(file);
}

static Error fifoCopyFunction(FifoFile* file, Process* process, VfsFile** ret) {
    increaseReferenceFor(file->name);
    FifoFile* copy = kalloc(sizeof(FifoFile));
    memcpy(copy, file, sizeof(FifoFile));
    *ret = (VfsFile*)copy;
    return simpleError(SUCCESS);
}

static const VfsFileVtable functions = {
    .read = (ReadFunction)fifoReadFunction,
    .write = (WriteFunction)fifoWriteFunction,
    .stat = (StatFunction)fifoStatFunction,
    .free = (FileFreeFunction)fifoFreeFunction,
    .copy = (CopyFunction)fifoCopyFunction,
};

FifoFile* createFifoFile(const char* path, VfsMode mode, Uid uid, Gid gid) {
    const char* name;
    PipeSharedData* data = getDataForName(path, &name);
    if (data == NULL || name == NULL) {
        return NULL;
    }
    FifoFile* file = zalloc(sizeof(FifoFile));
    if (file == NULL) {
        decreaseReferenceFor(name);
        return NULL;
    }
    file->base.functions = &functions;
    file->base.mode = mode;
    file->base.ino = 0;
    file->base.gid = gid;
    file->base.uid = uid;
    file->name = name;
    file->data = data;
    return file;
}

