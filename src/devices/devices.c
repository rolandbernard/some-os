
#include "devices/devices.h"

#include "devices/serial/uart16550.h"
#include "devices/virtio/block.h"
#include "devices/virtio/virtio.h"
#include "memory/memmap.h"
#include "files/blkfile.h"
#include "files/chrfile.h"
#include "files/vfs.h"
#include "util/text.h"

static Uart16550 serial_mmio;
static bool initialized;

Error initBaselineDevices() {
    // Initialize the uart device to enable logging
    serial_mmio.base_address = (void*)memory_map[VIRT_UART0].base;
    CHECKED(initUart16550(&serial_mmio));
    initialized = true;
    return simpleError(SUCCESS);
}

Error initDevices() {
    CHECKED(initVirtIODevices());
    return simpleError(SUCCESS);
}

Serial getDefaultSerialDevice() {
    if (!initialized) {
        initBaselineDevices();
    }
    return serialUart16550(&serial_mmio);
}

static Error mountVirtIOBlockDeviceFiles() {
    size_t blk_count = getDeviceCountOfType(VIRTIO_BLOCK);
    VirtIODevice* devs[blk_count];
    getDevicesOfType(VIRTIO_BLOCK, devs);
    for (size_t i = 0; i < blk_count; i++) {
        VirtIOBlockDevice* device = (VirtIOBlockDevice*)devs[i];
        VirtIOBlockDeviceLayout* info = (VirtIOBlockDeviceLayout*)device->virtio.mmio;
        VfsFile* file = (VfsFile*)createBlockDeviceFile(
            devs[i], info->config.blk_size, info->config.capacity * info->config.blk_size,
            (BlockOperationFunction)virtIOBlockDeviceOperation
        );
        FORMAT_STRINGX(name, "/dev/blk%li", i);
        CHECKED(mountFile(&global_file_system, file, name));
        KERNEL_LOG("[>] Mounted block device file %s", name);
    }
    return simpleError(SUCCESS);
}

static Error mountSerialDeviceFile() {
    VfsFile* file = (VfsFile*)createSerialDeviceFile(getDefaultSerialDevice());
    const char* name = "/dev/tty0";
    CHECKED(mountFile(&global_file_system, file, name));
    KERNEL_LOG("[>] Mounted serial device file %s", name);
    return simpleError(SUCCESS);
}

Error mountDeviceFiles() {
    // TODO: Maybe mount as a filesystem.
    CHECKED(mountVirtIOBlockDeviceFiles());
    CHECKED(mountSerialDeviceFile());
    return simpleError(SUCCESS);
}

