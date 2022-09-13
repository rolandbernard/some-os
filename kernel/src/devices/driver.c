
#include "devices/driver.h"

#include "devices/serial/uart16550.h"
#include "devices/virtio/virtio.h"
#include "interrupt/plic.h"
#include "interrupt/clint.h"

Error registerAllDrivers() {
    CHECKED(registerDriverUart16550());
    CHECKED(registerDriverClint());
    CHECKED(registerDriverPlic());
    CHECKED(registerDriverVirtIO());
    return simpleError(SUCCESS);
}

void registerDriver(Driver* driver) {

}

Driver* findDriverForNode(DeviceTreeNode* node) {
    return NULL;
}

Error initDriversForStdoutDevice() {
    return simpleError(ENOSYS);
}

Error initDriversForInterruptDevice() {
    return simpleError(ENOSYS);
}

Error initDriversForDeviceTreeNodes() {
    return simpleError(ENOSYS);
}

