#ifndef _INIT_H_
#define _INIT_H_

#include "error/error.h"

// Initialize all the kernel systems
Error initAllSystems();

// Initialize hart. Must run in Machine mode.
void initHart();

// Initialize the hart that is initializing the kernel.
void initPrimaryHart();

void initHarts();

#endif
