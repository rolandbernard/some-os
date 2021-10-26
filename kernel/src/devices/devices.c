
#include "devices/devices.h"

#include "devices/devfs.h"
#include "devices/serial/uart16550.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "memory/memmap.h"
#include "util/text.h"

static Uart16550 serial_mmio;
static bool initialized;

extern Error initBoardBaselineDevices();
extern Error initBoardDevices();

Error initBaselineDevices() {
    CHECKED(initBoardBaselineDevices());
    return simpleError(SUCCESS);
}

Error initDevices() {
    CHECKED(initBoardDevices());
    CHECKED(initVirtIODevices());
    return simpleError(SUCCESS);
}

Error mountDeviceFiles() {
    CHECKED(mountFilesystem(&global_file_system, (VfsFilesystem*)createDeviceFilesystem(), "/dev"))
    KERNEL_LOG("[>] Mounted device filesystem at /dev");
    return simpleError(SUCCESS);
}

