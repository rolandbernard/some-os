#ifndef _CHRFILE_H_
#define _CHRFILE_H_

#include "devices/serial/serial.h"
#include "files/vfs.h"
#include "util/spinlock.h"

// File wrapper around single byte read and write functions

typedef struct {
    VfsFile base;
    SpinLock lock;
    Serial serial;
} SerialDeviceFile;

SerialDeviceFile* createSerialDeviceFile(size_t ino, Serial serial);

#endif
