
#include "kernel/devtree.h"

#include "util/util.h"
#include "error/log.h"

Error initWithDeviceTree(uint8_t* dtb) {
    if (read32be(dtb) != 0xd00dfeed) {
        return someError(EINVAL, "Invalid device tree magic");
    }
    size_t size = read32be(dtb + 4);
    KERNEL_DEBUG("dtb size: %lu", size);
    return simpleError(ENOSYS);
}

