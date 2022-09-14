#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <stdint.h>

#include "error/error.h"

void kernelInit(uint8_t* dtb);

// Initialize hart. Must run in Machine mode.
Error initHart(int hartid);

// Initialize the hart that is initializing the kernel.
Error initPrimaryHart();

#endif
