#ifndef _INIT_H_
#define _INIT_H_

#include "error/error.h"

// Initialize all the kernel systems that are not in the basic set
Error initAllSystems();

// Initialize hart. Must run in Machine mode.
void initHart(int hartid);

// Initialize the hart that is initializing the kernel.
void initPrimaryHart();

#endif
