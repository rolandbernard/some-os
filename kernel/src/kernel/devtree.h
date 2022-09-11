#ifndef _KERNEL_DEVTREE_H_
#define _KERNEL_DEVTREE_H_

#include <stdint.h>

#include "error/error.h"

Error initWithDeviceTree(uint8_t* dtb);

#endif
